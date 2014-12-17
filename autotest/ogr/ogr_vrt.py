#!/usr/bin/env python
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
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import test_cli_utilities

###############################################################################
# Open VRT datasource.

def ogr_vrt_1():

    gdaltest.vrt_ds = ogr.Open( 'data/vrt_test.vrt' )
    
    if gdaltest.vrt_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify the geometries, in the "test2" layer based on x,y,z columns.
#
# Also tests FID-copied-from-source.

def ogr_vrt_2():
    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test2' )

    extent = lyr.GetExtent()
    if extent != (12.5, 100.0, 17.0, 200.0):
        gdaltest.post_reason('wrong extent')
        print(extent)
        return 'fail'

    expect = ['First', 'Second']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(12.5 17 1.2)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(100 200)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 1:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# Same test on layer 3 derived from WKT column.
#
# Also tests FID-from-attribute.

def ogr_vrt_3():
    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test3' )

    expect = ['First', 'Second']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(12.5 17 1.2)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 1:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(100 200)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 2:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    return 'success'

    
###############################################################################
# Test a spatial query. 

def ogr_vrt_4():
    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test3' )
    lyr.ResetReading()

    lyr.SetSpatialFilterRect( 90, 90, 300, 300 )
    
    expect = ['Second']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(100 200)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    feat.Destroy()

    lyr.SetSpatialFilter( None )
    
    return 'success'

    
###############################################################################
# Test an attribute query. 

def ogr_vrt_5():

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test3' )
    lyr.ResetReading()

    lyr.SetAttributeFilter( 'x < 50' )
    
    expect = ['First']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(12.5 17 1.2)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    feat.Destroy()

    lyr.SetAttributeFilter( None )
    
    return 'success'

###############################################################################
# Test GetFeature() on layer with FID coming from a column.

def ogr_vrt_6():

    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test3' )
    lyr.ResetReading()
    
    feat = lyr.GetFeature( 2 )
    if feat.GetField( 'other' ) != 'Second':
        gdaltest.post_reason( 'GetFeature() did not work properly.' )
        return 'fail'
    
    feat.Destroy()

    return 'success'
    
###############################################################################
# Same as test 3, but on the result of an SQL query.
#

def ogr_vrt_7():
    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test4' )

    expect = ['First', 'Second']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(12.5 17 1.2)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 1:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(100 200)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 2:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    return 'success'

    
###############################################################################
# Similar test, but now we put the whole VRT contents directly into the
# "filename". 
#

def ogr_vrt_8():
    if gdaltest.vrt_ds is None:
        return 'skip'

    vrt_xml = '<OGRVRTDataSource><OGRVRTLayer name="test4"><SrcDataSource relativeToVRT="0">data/flat.dbf</SrcDataSource><SrcSQL>SELECT * FROM flat</SrcSQL><FID>fid</FID><GeometryType>wkbPoint</GeometryType><GeometryField encoding="PointFromColumns" x="x" y="y" z="z"/></OGRVRTLayer></OGRVRTDataSource>'
    ds = ogr.Open( vrt_xml )
    lyr = ds.GetLayerByName( 'test4' )

    expect = ['First', 'Second']
    
    tr = ogrtest.check_features_against_list( lyr, 'other', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(12.5 17 1.2)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 1:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()
    
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,'POINT(100 200)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    if feat.GetFID() != 2:
        gdaltest.post_reason( 'Unexpected fid' )
        return 'fail'

    feat.Destroy()

    ds.Destroy()
    ds = None
    
    return 'success'

    
###############################################################################
# Test that attribute filters are passed through to an underlying layer.

def ogr_vrt_9():

    if gdaltest.vrt_ds is None:
        return 'skip'

    lyr = gdaltest.vrt_ds.GetLayerByName( 'test3' )
    lyr.SetAttributeFilter( 'other = "Second"' )
    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if feat.GetField( 'other' ) != 'Second':
        gdaltest.post_reason( 'attribute filter did not work.' )
        return 'fail'
    
    feat.Destroy()

    sub_ds = ogr.OpenShared( 'data/flat.dbf' )
    sub_layer = sub_ds.GetLayerByName( 'flat' )

    sub_layer.ResetReading()
    if sub_layer.GetFeatureCount() != 1:
        print(sub_layer.GetFeatureCount())
        gdaltest.post_reason( 'attribute filter not passed to sublayer.' )
        return 'fail'

    lyr.SetAttributeFilter( None )

    sub_ds.Release()
    sub_ds = None

    return 'success'
    
###############################################################################
# Test capabilities
#

def ogr_vrt_10():
    if gdaltest.vrt_ds is None:
        return 'skip'

    vrt_xml = '<OGRVRTDataSource><OGRVRTLayer name="test"><SrcDataSource relativeToVRT="0">data/testpoly.shp</SrcDataSource><SrcLayer>testpoly</SrcLayer></OGRVRTLayer></OGRVRTDataSource>'
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )
    src_ds = ogr.Open('data/testpoly.shp')
    src_lyr = src_ds.GetLayer(0)
    
    if vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) != src_lyr.TestCapability(ogr.OLCFastFeatureCount):
        return 'fail'
    if vrt_lyr.TestCapability(ogr.OLCFastGetExtent) != src_lyr.TestCapability(ogr.OLCFastGetExtent):
        return 'fail'
    if vrt_lyr.TestCapability(ogr.OLCRandomRead) != src_lyr.TestCapability(ogr.OLCRandomRead):
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None
    src_ds.Destroy()
    src_ds = None
    
    return 'success'

###############################################################################
# Test VRT write capabilities with PointFromColumns geometries
# Test also the reportGeomSrcColumn attribute

def ogr_vrt_11():
    if gdaltest.vrt_ds is None:
        return 'skip'

    f = open('tmp/test.csv', 'wb')
    f.write('x,val1,y,val2,style\n'.encode('ascii'))
    f.write('2,"val11",49,"val12","PEN(c:#FF0000,w:5pt,p:""2px 1pt"")"\n'.encode('ascii'))
    f.close()

    try:
        os.remove('tmp/test.csvt')
    except:
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
    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    # Only val1, val2, style attributes should be reported
    if vrt_lyr.GetLayerDefn().GetFieldCount() != 3:
        gdaltest.post_reason('failure')
        print(vrt_lyr.GetLayerDefn().GetFieldCount())
        return 'fail'
    if vrt_lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() != 'val1':
        gdaltest.post_reason('failure')
        return 'fail'
    if vrt_lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef() != 'val2':
        gdaltest.post_reason('failure')
        return 'fail'

    feat = vrt_lyr.GetNextFeature()
    if feat.GetStyleString() != 'PEN(c:#FF0000,w:5pt,p:"2px 1pt")':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)
    feat.Destroy()

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(2)
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    if feat.GetFieldAsString('val1') != 'val21':
        gdaltest.post_reason('failure')
        return 'fail'
    feat.Destroy()

    # The x and y fields are considered as string by default, so spatial
    # filter cannot be turned into attribute filter
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    vrt_lyr.SetSpatialFilterRect(0, 40, 10, 49.5)
    ret = vrt_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find('not declared as numeric fields') == -1:
        gdaltest.post_reason('failure')
        return 'fail'
    if ret != 1:
        gdaltest.post_reason('failure')
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    # Add a .csvt file to specify the x and y columns as reals
    f = open('tmp/test.csvt', 'wb')
    f.write('Real,String,Real,String\n'.encode('ascii'))
    f.close()

    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )
    vrt_lyr.SetSpatialFilterRect(0, 40, 10, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('failure')
        return 'fail'

    vrt_lyr.SetAttributeFilter("1 = 1")
    if vrt_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'

    vrt_lyr.SetAttributeFilter("1 = 0")
    if vrt_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    os.remove('tmp/test.csv')
    os.remove('tmp/test.csvt')

    return 'success'

###############################################################################
# Test VRT write capabilities with WKT geometries

def ogr_vrt_12():
    if gdaltest.vrt_ds is None:
        return 'skip'

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
    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)
    feat.Destroy()

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(2)
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        return 'fail'
    if feat.GetFieldAsString('val1') != 'val21':
        return 'fail'
    feat.Destroy()

    vrt_ds.Destroy()
    vrt_ds = None

    os.remove('tmp/test.csv')

    return 'success'

###############################################################################
# Test VRT write capabilities with WKB geometries

def ogr_vrt_13():
    if gdaltest.vrt_ds is None:
        return 'skip'

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
    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    feat = ogr.Feature(vrt_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometryDirectly(geom)
    feat.SetField('val1', 'val21')
    vrt_lyr.CreateFeature(feat)
    feat.Destroy()

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetFeature(1)
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        return 'fail'
    if feat.GetFieldAsString('val1') != 'val21':
        return 'fail'
    feat.Destroy()

    vrt_ds.Destroy()
    vrt_ds = None

    os.remove('tmp/test.csv')

    return 'success'

###############################################################################
# Test SrcRegion element for VGS_Direct

def ogr_vrt_14():
    if gdaltest.vrt_ds is None:
        return 'skip'
    
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
    except:
        pass
    gdal.PopErrorHandler()
    
    shp_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test.shp')
    shp_lyr = shp_ds.CreateLayer('test')

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    shp_ds.ExecuteSQL('CREATE SPATIAL INDEX on test');

    shp_ds.Destroy()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="mytest">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <SrcRegion>POLYGON((0 40,0 50,10 50,10 40,0 40))</SrcRegion>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'mytest' )

    if vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) != 1:
        gdaltest.post_reason( 'Fast filter not set.' )
        return 'fail'

    extent = vrt_lyr.GetExtent()
    if extent != (2.0, 2.0, 49.0, 49.0):
        gdaltest.post_reason('wrong extent')
        print(extent)
        return 'fail'

    if vrt_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'Feature count not one as expected.' )
        return 'fail'

    feat = vrt_lyr.GetNextFeature()
    if feat.GetFID() != 2:
        gdaltest.post_reason( 'did not get fid 2.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason( 'did not get expected point geometry.' )
        return 'fail'
    feat.Destroy()

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
            return 'skip'
        
        print(vrt_lyr.GetFeatureCount())
        gdaltest.post_reason( 'did not get one feature on rect spatial filter.' )
        return 'fail'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    if vrt_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason( 'Did not get expected zero feature count.')
        return 'fail'

    vrt_lyr.SetSpatialFilter(None)
    if vrt_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'Did not get expected one feature count with no filter.')
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')

    return 'success'


###############################################################################
# Test SrcRegion element for VGS_WKT

def ogr_vrt_15():
    if gdaltest.vrt_ds is None:
        return 'skip'

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
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    if vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0:
        return 'fail'

    if vrt_lyr.GetFeatureCount() != 1:
        return 'fail'

    feat = vrt_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        return 'fail'
    feat.Destroy()

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        return 'fail'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    if vrt_lyr.GetFeatureCount() != 0:
        return 'fail'

    vrt_lyr.SetSpatialFilter(None)
    if vrt_lyr.GetFeatureCount() != 1:
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    os.remove('tmp/test.csv')

    return 'success'


###############################################################################
# Test SrcRegion element for VGS_PointFromColumns

def ogr_vrt_16():
    if gdaltest.vrt_ds is None:
        return 'skip'

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
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    if vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0:
        return 'fail'

    if vrt_lyr.GetFeatureCount() != 1:
        return 'fail'

    feat = vrt_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        return 'fail'
    feat.Destroy()

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            vrt_ds.Destroy()
            os.remove('tmp/test.csv')
            os.remove('tmp/test.csvt')
            return 'skip'
        return 'fail'

    vrt_lyr.SetSpatialFilterRect(1, 41, 3, 48.5)
    if vrt_lyr.GetFeatureCount() != 0:
        return 'fail'

    vrt_lyr.SetSpatialFilter(None)
    if vrt_lyr.GetFeatureCount() != 1:
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    os.remove('tmp/test.csv')
    os.remove('tmp/test.csvt')

    return 'success'


###############################################################################
# Test explicit field definitions.

def ogr_vrt_17():

    if gdaltest.vrt_ds is None:
        return 'skip'

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
        
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    if vrt_lyr.GetLayerDefn().GetFieldCount() != 4:
        gdaltest.post_reason( 'unexpected field count.' )
        return 'fail'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(0)
    if flddef.GetName() != 'pm_code' \
       or flddef.GetType() != ogr.OFTInteger \
       or flddef.GetWidth() != 4 \
       or flddef.GetPrecision() != 0:
        gdaltest.post_reason( 'pm_code field definition wrong.' )
        return 'fail'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(1)
    if flddef.GetName() != 'prime_meridian_name' \
       or flddef.GetType() != ogr.OFTString \
       or flddef.GetWidth() != 24 \
       or flddef.GetPrecision() != 0:
        gdaltest.post_reason( 'prime_meridian_name field definition wrong.' )
        return 'fail'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(2)
    if flddef.GetName() != 'new_col' \
       or flddef.GetType() != ogr.OFTReal \
       or flddef.GetWidth() != 12 \
       or flddef.GetPrecision() != 3:
        gdaltest.post_reason( 'new_col field definition wrong.' )
        return 'fail'

    flddef = vrt_lyr.GetLayerDefn().GetFieldDefn(3)
    if flddef.GetName() != 'DEPRECATED' \
       or flddef.GetType() != ogr.OFTInteger \
       or flddef.GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason( 'DEPRECATED field definition wrong.' )
        return 'fail'

    feat = vrt_lyr.GetNextFeature()

    if feat.GetField(0) != 8901 or feat.GetField(1) != "Greenwich" \
       or feat.GetField(2) != None:
        gdaltest.post_reason( 'did not get expected field value(s).' )
        return 'fail'
    
    feat.Destroy()
    
    vrt_ds.Destroy()
    vrt_ds = None

    return 'success'

###############################################################################
# Test that attribute filters are *not* passed to sublayer by default
# when explicit fields are defined.

def ogr_vrt_18():

    if gdaltest.vrt_ds is None:
        return 'skip'

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
        
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )
    vrt_lyr.SetAttributeFilter( 'pm_code=8904' )
    
    feat = vrt_lyr.GetNextFeature()

    if feat.GetField(0) != 8904:
        gdaltest.post_reason( 'Attribute filter not working properly' )
        return 'fail'
    
    feat.Destroy()
    
    vrt_ds.Destroy()
    vrt_ds = None

    return 'success'

###############################################################################
# Run test_ogrsf (optimized path)

def ogr_vrt_19_optimized():

    if gdaltest.vrt_ds is None:
        return 'skip'

    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/poly_vrt.vrt')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf (non optimized path)

def ogr_vrt_19_nonoptimized():

    if gdaltest.vrt_ds is None:
        return 'skip'

    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/poly_nonoptimized_vrt.vrt')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test VGS_Direct

def ogr_vrt_20():
    if gdaltest.vrt_ds is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
    except:
        pass
    gdal.PopErrorHandler()

    shp_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/test.shp')
    shp_lyr = shp_ds.CreateLayer('test')

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 45)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (-10 49)')
    feat.SetGeometryDirectly(geom)
    shp_lyr.CreateFeature(feat)
    feat.Destroy()

    shp_ds.ExecuteSQL('CREATE SPATIAL INDEX on test');

    shp_ds.Destroy()

    vrt_xml = """
<OGRVRTDataSource>
    <OGRVRTLayer name="mytest">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'mytest' )

    if vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason( 'Fast feature count not set.' )
        return 'fail'

    if vrt_lyr.TestCapability(ogr.OLCFastSpatialFilter) != 1:
        gdaltest.post_reason( 'Fast filter not set.' )
        return 'fail'

    if vrt_lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
        gdaltest.post_reason( 'Fast extent not set.' )
        return 'fail'

    extent = vrt_lyr.GetExtent()
    if extent != (-10.0, 2.0, 45.0, 49.0):
        gdaltest.post_reason('wrong extent')
        print(extent)
        return 'fail'

    if vrt_lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'Feature count not 4 as expected.' )
        return 'fail'

    vrt_lyr.SetSpatialFilterRect(1, 48.5, 3, 49.5)
    if vrt_lyr.GetFeatureCount() != 1:
        if gdal.GetLastErrorMsg().find('GEOS support not enabled') != -1:
            ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')
            return 'skip'

        print(vrt_lyr.GetFeatureCount())
        gdaltest.post_reason( 'did not get one feature on rect spatial filter.' )
        return 'fail'

    if vrt_lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason( 'Fast feature count not set.' )
        return 'fail'

    if vrt_lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
        gdaltest.post_reason( 'Fast extent not set.' )
        return 'fail'

    extent = vrt_lyr.GetExtent()
    # the shapefile driver currently doesn't change the extent even in the
    # presence of a spatial filter, so that could change in the future
    if extent != (-10.0, 2.0, 45.0, 49.0):
        gdaltest.post_reason('wrong extent')
        print(extent)
        return 'fail'

    vrt_lyr.SetSpatialFilterRect(1, 48, 3, 48.5)
    if vrt_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason( 'Did not get expected zero feature count.')
        return 'fail'

    vrt_lyr.SetSpatialFilter(None)
    if vrt_lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'Feature count not 4 as expected with no filter.')
        return 'fail'

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetNextFeature()
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    vrt_lyr.SetFeature(feat)
    feat = None

    vrt_lyr.ResetReading()
    feat = vrt_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason( 'fail')
        feat.DumpReadable()
        return 'fail'

    vrt_ds.Destroy()
    vrt_ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test.shp')

    return 'success'

###############################################################################
# Test lazy initialization with valid layer

def ogr_vrt_21_internal():

    if gdaltest.vrt_ds is None:
        return 'skip'

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetName() != 'test3':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetSpatialRef().ExportToWkt().find('84') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.ResetReading()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetNextFeature() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFeature(1) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFeatureCount() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.SetNextByIndex(1) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetLayerDefn().GetFieldCount() == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.SetAttributeFilter('') != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SetSpatialFilter(None)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetExtent() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFIDColumn() != 'fid':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    feature_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feature_defn)
    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.DeleteFeature(0)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SyncToDisk()
    ds = None

    return 'success'

def ogr_vrt_21():
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
        return 'skip'

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetName() != 'test5':
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetSpatialRef().ExportToWkt().find('84') == -1:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.ResetReading()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetNextFeature() is not None:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetFeature(1) is not None:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetFeatureCount() != 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.SetNextByIndex(1) == 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetLayerDefn().GetFieldCount() != 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.SetAttributeFilter('') == 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SetSpatialFilter(None)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    if lyr.GetFIDColumn() != '':
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.GetExtent()
    ds = None

    feature_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feature_defn)
    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.DeleteFeature(0)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.SyncToDisk()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.StartTransaction()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.CommitTransaction()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test5')
    lyr.RollbackTransaction()
    ds = None

    return 'success'

def ogr_vrt_22():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        ret = ogr_vrt_22_internal()
    except:
        ret = 'fail'
    gdal.PopErrorHandler()
    return ret

###############################################################################
# Test anti-recursion mechanism

def ogr_vrt_23(shared_ds_flag = ''):

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

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
    if ds is None:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetLayer(0).GetLayerDefn()
    ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('error expected !')
        return 'fail'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

    return 'success'

###############################################################################
# Test anti-recursion mechanism on shared DS

def ogr_vrt_24():

    return ogr_vrt_23(' shared="1"')


###############################################################################
# Test GetFIDColumn() (#4637)

def ogr_vrt_25():

    ds = ogr.Open('data/vrt_test.vrt')

    # test3 layer just declares fid, and implicit fields (so all source
    # fields are taken as VRT fields), we can report the fid column 
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFIDColumn() != 'fid':
        return 'fail'

    # test3 layer just declares fid, and explicit fields without the fid
    # column, so we can *not* report it
    lyr = ds.GetLayerByName('test6')
    if lyr.GetFIDColumn() != '':
        return 'fail'

    # test2 layer does not declare fid, and source layer has no fid column
    # so nothing to report
    lyr = ds.GetLayerByName('test2')
    if lyr.GetFIDColumn() != '':
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test transaction support

def ogr_vrt_26():

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    sqlite_ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_vrt_26.db')
    if sqlite_ds is None:
        return 'skip'

    lyr = sqlite_ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr = None

    vrt_ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource>/vsimem/ogr_vrt_26.db</SrcDataSource>
    </OGRVRTLayer>
</OGRVRTDataSource>""", update = 1)

    lyr = vrt_ds.GetLayer(0)
    if lyr.TestCapability(ogr.OLCTransactions) == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.StartTransaction()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo')
    lyr.CreateFeature(feat)
    feat = None

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.RollbackTransaction()

    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.StartTransaction()
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)
    feat = None
    lyr.CommitTransaction()

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failed')
        return 'fail'

    vrt_ds = None

    sqlite_ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_vrt_26.db')

    return 'success'

###############################################################################
# Test shapebin geometry

def ogr_vrt_27():

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

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    wkt_list = [ 'POINT (1 2)', 'LINESTRING (3 4,5 6)', 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' ]

    feat = lyr.GetNextFeature()
    i = 0
    while feat is not None:
        if ogrtest.check_feature_geometry(feat, wkt_list[i]) != 0:
            return 'fail'
        feat = lyr.GetNextFeature()
        i = i + 1

    ds = None

    gdal.Unlink('/vsimem/ogr_vrt_27.csv')

    return 'success'

###############################################################################
# Invalid VRT testing

def ogr_vrt_28():

    ds = ogr.Open("<OGRVRTDataSource></foo>")
    if ds is not None:
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/ogr_vrt_28_invalid.vrt', "<bla><OGRVRTDataSource></OGRVRTDataSource></bla>")
    ds = ogr.Open("/vsimem/ogr_vrt_28_invalid.vrt")
    if ds is not None:
        return 'fail'
    gdal.Unlink("/vsimem/ogr_vrt_28_invalid.vrt")

    ds = ogr.Open("data/invalid.vrt")
    if ds is None:
        return 'fail'

    for i in range(ds.GetLayerCount()):
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        lyr = ds.GetLayer(i)
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('expected failure for layer %d of datasource %s' % (i, ds.GetName()))
            return 'fail'

    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("<OGRVRTDataSource><OGRVRTLayer/></OGRVRTDataSource>")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('expected datasource opening failure')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("data/invalid2.vrt")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('expected datasource opening failure')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("data/invalid3.vrt")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('expected datasource opening failure')
        return 'fail'

    return 'success'

###############################################################################
# Test OGRVRTWarpedLayer

def ogr_vrt_29():

    try:
        os.unlink('tmp/ogr_vrt_29.shp')
        os.unlink('tmp/ogr_vrt_29.shx')
        os.unlink('tmp/ogr_vrt_29.dbf')
        os.unlink('tmp/ogr_vrt_29.prj')
    except:
        pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_vrt_29.shp')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('ogr_vrt_29', srs = sr)
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
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/non_existing.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Non-spatial layer
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="flat">
            <SrcDataSource>data/flat.dbf</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Missing TargetSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid TargetSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <TargetSRS>foo</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid SrcSRS
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open("""<OGRVRTDataSource>
    <OGRVRTWarpedLayer>
        <OGRVRTLayer name="ogr_vrt_29">
            <SrcDataSource>tmp/ogr_vrt_29.shp</SrcDataSource>
        </OGRVRTLayer>
        <SrcSRS>foo</SrcSRS>
        <TargetSRS>EPSG:32631</TargetSRS>
    </OGRVRTWarpedLayer>
</OGRVRTDataSource>""")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if bb != ( 426857, 485608, 5427475, 5516874 ):
        gdaltest.post_reason('fail')
        return 'fail'
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
    ds = ogr.Open('tmp/ogr_vrt_29.vrt', update = 1)
    lyr = ds.GetLayer(0)

    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    if got_wkt.find('32631') == -1:
        gdaltest.post_reason('did not get expected WKT')
        print(got_wkt)
        return 'fail'

    bb = lyr.GetExtent()
    expected_bb = (426857.98771727527, 485607.2165091355, 5427475.0501426803, 5516873.8591036052)

    for i in range(4):
        if abs(bb[i] - expected_bb[i]) > 1:
            gdaltest.post_reason('did not get expected extent')
            print(bb)
            return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(426857.987717275274917 5427937.523466162383556)') != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()

    feat.SetGeometry(None)
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    lyr.SetSpatialFilterRect(499999,-1,500001,1)
    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(500000 0)') != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('id', -99)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('id', -100)
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    ds = None

    # Check failed operations in read-only
    ds = ogr.Open('tmp/ogr_vrt_29.vrt')
    lyr = ds.GetLayer(0)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(1)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    ds = None

    # Check in .shp file
    ds = ogr.Open('tmp/ogr_vrt_29.shp', update = 1)
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(3.0 0.0)') != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetAttributeFilter('id = -99')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT(3.0 0.0)') != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.SetAttributeFilter('id = -100')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1000)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 0)'))
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    # Check failed reprojection when reading through VRT
    ds = ogr.Open('tmp/ogr_vrt_29.vrt', update = 1)
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter('id = 1000')

    # Reprojection will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    fid = feat.GetFID()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetFeature(fid)
    gdal.PopErrorHandler()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
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
    ds = ogr.Open('tmp/ogr_vrt_29_2.vrt', update = 1)
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-180 0)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    ds = None

    # Some sanity operations before running test_ogrsf
    ds = ogr.Open('tmp/ogr_vrt_29.shp', update = 1)
    ds.ExecuteSQL('REPACK ogr_vrt_29')
    ds.ExecuteSQL('RECOMPUTE EXTENT ON ogr_vrt_29')
    ds = None

    # Check with test_ogrsf
    if test_cli_utilities.get_test_ogrsf_path() is not None:

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_29.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

    try:
        os.unlink('tmp/ogr_vrt_29.shp')
        os.unlink('tmp/ogr_vrt_29.shx')
        os.unlink('tmp/ogr_vrt_29.dbf')
        os.unlink('tmp/ogr_vrt_29.prj')
    except:
        pass
    os.unlink('tmp/ogr_vrt_29.vrt')
    os.unlink('tmp/ogr_vrt_29_2.vrt')

    return 'success'


###############################################################################
# Test OGRVRTUnionLayer

def ogr_vrt_30():

    for filename in [ 'tmp/ogr_vrt_30_1.shp',
                      'tmp/ogr_vrt_30_1.shx',
                      'tmp/ogr_vrt_30_1.dbf',
                      'tmp/ogr_vrt_30_1.prj',
                      'tmp/ogr_vrt_30_1.qix',
                      'tmp/ogr_vrt_30_2.shp',
                      'tmp/ogr_vrt_30_2.shx',
                      'tmp/ogr_vrt_30_2.dbf',
                      'tmp/ogr_vrt_30_2.prj',
                      'tmp/ogr_vrt_30_2.qix' ]:
        try:
            os.unlink(filename)
        except:
            pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_vrt_30_1.shp')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('ogr_vrt_30_1', srs = sr)
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
    lyr = ds.CreateLayer('ogr_vrt_30_2', srs = sr)
    lyr.CreateField(ogr.FieldDefn('id2', ogr.OFTInteger))
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
        ds = ogr.Open('tmp/ogr_vrt_30.vrt', update = 1)
        lyr = ds.GetLayer(0)

        if check == 0:
            sr = lyr.GetSpatialRef()
            got_wkt = sr.ExportToWkt()
            if got_wkt.find('GEOGCS["GCS_WGS_1984"') == -1:
                gdaltest.post_reason('did not get expected WKT')
                print(got_wkt)
                return 'fail'
        elif check == 1:
            bb = lyr.GetExtent()
            expected_bb = (2.0, 4.7999999999999998, 49.0, 49.799999999999997)

            for i in range(4):
                if abs(bb[i] - expected_bb[i]) > 1:
                    gdaltest.post_reason('did not get expected extent')
                    print(bb)
                    return 'fail'
        elif check == 2:
            feat_count = lyr.GetFeatureCount()
            if feat_count != 2 * 5 * 5:
                gdaltest.post_reason('did not get expected feature count')
                print(feat_count)
                return 'fail'
        elif check == 3:
            if lyr.GetLayerDefn().GetFieldCount() != 3:
                gdaltest.post_reason('did not get expected field count')
                return 'fail'
        elif check == 4:
            feat = lyr.GetNextFeature()
            i = 0
            while feat is not None:
                if i < 5 * 5:
                    if feat.GetFID() != i:
                        gdaltest.post_reason('did not get expected value')
                        print(feat.GetFID())
                        return 'fail'
                    if feat.GetFieldAsInteger("id1") != i:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.GetFieldAsInteger("id2") != 100 + i:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.IsFieldSet("id3"):
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if ogrtest.check_feature_geometry(feat, 'POINT(%f %f)' % (2 + int(i / 5) / 5.0, 49 + int(i % 5) / 5.0)) != 0:
                        gdaltest.post_reason('did not get expected value')
                        feat.DumpReadable()
                        return 'fail'
                else:
                    if feat.GetFID() != i:
                        gdaltest.post_reason('did not get expected value')
                        print(feat.GetFID())
                        return 'fail'
                    if feat.IsFieldSet("id1"):
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.GetFieldAsInteger("id2") != 200 + i - 5 * 5:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.GetFieldAsInteger("id3") != 300 + i - 5 * 5:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if ogrtest.check_feature_geometry(feat, 'POINT(%f %f)' % (4 + int((i - 5 * 5) / 5) / 5.0, 49 + int((i - 5 * 5) % 5) / 5.0)) != 0:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'

                i = i + 1
                feat = lyr.GetNextFeature()

        elif check == 5:
            if lyr.GetGeomType() != ogr.wkbPoint:
                gdaltest.post_reason('did not get expected geom type')
                return 'fail'

        elif check == 6:
            if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCIgnoreFields) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCRandomWrite) != 0:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCSequentialWrite) != 0:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

        elif check == 7:
            lyr.SetSpatialFilterRect(2.49, 49.29, 4.49, 49.69)
            if lyr.GetFeatureCount() != 10:
                print(lyr.GetFeatureCount())
                gdaltest.post_reason('did not get expected feature count')
                return 'fail'

        elif check == 8:
            lyr.SetAttributeFilter('id1 = 0')
            if lyr.GetFeatureCount() != 1:
                print(lyr.GetFeatureCount())
                gdaltest.post_reason('did not get expected feature count')
                return 'fail'

        elif check == 9:
            # CreateFeature() should fail
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # SetFeature() should fail
            lyr.ResetReading()
            feat = lyr.GetNextFeature()
            feat.SetField('id2', 45321)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # Test feature existence : should fail
            lyr.SetAttributeFilter('id2 = 12345 or id2 = 45321')
            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            if feat is not None:
                gdaltest.post_reason('should have failed')
                return 'fail'

        ds = None

    # Check with test_ogrsf
    if test_cli_utilities.get_test_ogrsf_path() is not None:

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_30.vrt --config OGR_VRT_MAX_OPENED 1')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_vrt_30.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
        ds = ogr.Open('tmp/ogr_vrt_30.vrt', update = 1)
        lyr = ds.GetLayer(0)

        if check == 0:
            sr = lyr.GetSpatialRef()
            got_wkt = sr.ExportToWkt()
            if got_wkt.find('WGS 72') == -1:
                gdaltest.post_reason('did not get expected WKT')
                print(got_wkt)
                return 'fail'

        elif check == 1:
            bb = lyr.GetExtent()
            expected_bb = (-180.0, 180.0, -90.0, 90.0)

            for i in range(4):
                if abs(bb[i] - expected_bb[i]) > 1:
                    gdaltest.post_reason('did not get expected extent')
                    print(bb)
                    return 'fail'

        elif check == 2:
            if lyr.GetFeatureCount() != 100:
                gdaltest.post_reason('did not get expected feature count')
                return 'fail'

        elif check == 3:
            if lyr.GetLayerDefn().GetFieldCount() != 2:
                gdaltest.post_reason('did not get expected field count')
                return 'fail'

        elif check == 4:
            feat = lyr.GetNextFeature()
            i = 0
            while feat is not None:
                if i < 5 * 5:
                    if feat.GetFID() != i:
                        gdaltest.post_reason('did not get expected value')
                        print(feat.GetFID())
                        return 'fail'
                    if feat.GetFieldAsString("source_layer") != 'ogr_vrt_30_1':
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.GetFieldAsInteger("id2") != 100 + i:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'

                else:
                    if feat.GetFID() != i - 5 * 5:
                        gdaltest.post_reason('did not get expected value')
                        print(feat.GetFID())
                        return 'fail'
                    if feat.GetFieldAsString("source_layer") != 'ogr_vrt_30_2':
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'
                    if feat.GetFieldAsInteger("id2") != 200 + i - 5 * 5:
                        gdaltest.post_reason('did not get expected value')
                        return 'fail'

                i = i + 1
                feat = lyr.GetNextFeature()

        elif check == 5:
            if lyr.GetGeomType() != ogr.wkbPoint25D:
                gdaltest.post_reason('did not get expected geom type')
                return 'fail'

        elif check == 6:
            if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCIgnoreFields) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCRandomWrite) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

            if lyr.TestCapability(ogr.OLCSequentialWrite) != 1:
                gdaltest.post_reason('did not get expected capability')
                return 'fail'

        elif check == 7:
            lyr.SetSpatialFilterRect(2.49, 49.29, 4.49, 49.69)
            if lyr.GetFeatureCount() != 10:
                gdaltest.post_reason('did not get expected feature count')
                return 'fail'

        elif check == 8:
            # invalid source_layer name with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'random_name')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # unset source_layer name with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # FID set with CreateFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetFID(999999)
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.CreateFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # CreateFeature() OK
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            if lyr.CreateFeature(feat) != 0:
                gdaltest.post_reason('should have succeeded')
                return 'fail'

            # SetFeature() OK
            feat.SetField('id2', 45321)
            if lyr.SetFeature(feat) != 0:
                gdaltest.post_reason('should have succeeded')
                return 'fail'

            # invalid source_layer name with SetFeature()
            feat.SetField('source_layer', 'random_name')
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'

            # unset source_layer name with SetFeature()
            feat.UnsetField('source_layer')
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'

            # FID unset with SetFeature()
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('source_layer', 'ogr_vrt_30_2')
            feat.SetField('id2', 12345)
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.SetFeature(feat)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('should have failed')
                return 'fail'
            feat = None

            # Test feature existence (with passthru)
            lyr.SetAttributeFilter('id2 = 45321 AND OGR_GEOMETRY IS NULL')

            if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
                gdaltest.post_reason('should have returned 1')
                return 'fail'

            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            if feat is None:
                gdaltest.post_reason('should have succeeded')
                return 'fail'

            # Test feature existence (without passthru)
            lyr.SetAttributeFilter("id2 = 45321 AND OGR_GEOMETRY IS NULL AND source_layer = 'ogr_vrt_30_2'")

            if lyr.TestCapability(ogr.OLCFastFeatureCount) != 0:
                gdaltest.post_reason('should have returned 0')
                return 'fail'

            lyr.ResetReading()

            feat = lyr.GetNextFeature()
            if feat is None:
                gdaltest.post_reason('should have succeeded')
                return 'fail'

            # Test SyncToDisk()
            if lyr.SyncToDisk() != 0:
                gdaltest.post_reason('should have succeeded')
                return 'fail'

        ds = None

    for filename in [ 'tmp/ogr_vrt_30_1.shp',
                      'tmp/ogr_vrt_30_1.shx',
                      'tmp/ogr_vrt_30_1.dbf',
                      'tmp/ogr_vrt_30_1.prj',
                      'tmp/ogr_vrt_30_1.qix',
                      'tmp/ogr_vrt_30_2.shp',
                      'tmp/ogr_vrt_30_2.shx',
                      'tmp/ogr_vrt_30_2.dbf',
                      'tmp/ogr_vrt_30_2.prj',
                      'tmp/ogr_vrt_30_2.qix' ]:
        try:
            os.unlink(filename)
        except:
            pass
    os.unlink('tmp/ogr_vrt_30.vrt')

    return 'success'

###############################################################################
# Test anti-recursion mechanism with union layer

def ogr_vrt_31(shared_ds_flag = ''):

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
    if ds is None:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.GetLayer(0).GetLayerDefn()
    ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('error expected !')
        return 'fail'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

    return 'success'

###############################################################################
# Test anti-recursion mechanism on shared DS

def ogr_vrt_32():

    return ogr_vrt_31(' shared="1"')


###############################################################################
# Test multi-geometry support

def ogr_vrt_33():

    try:
        import shutil
        shutil.rmtree( 'tmp/ogr_vrt_33' )
    except:
        pass

    ds = ogr.GetDriverByName('CSV').CreateDataSource('tmp/ogr_vrt_33', options = ['GEOMETRY=AS_WKT'])
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone, options = ['CREATE_CSVT=YES'] )
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_POINT", ogr.wkbPoint))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32632_POLYGON", ogr.wkbPolygon))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_LINESTRING", ogr.wkbLineString))
    lyr.CreateField(ogr.FieldDefn("X", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("Y", ogr.OFTReal))
    
    lyr = ds.CreateLayer('test2', geom_type = ogr.wkbNone, options = ['CREATE_CSVT=YES'] )
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32632_POLYGON", ogr.wkbPolygon))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_4326_POINT", ogr.wkbPoint))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_EPSG_32631_POINT", ogr.wkbPoint))
    lyr.CreateField(ogr.FieldDefn("Y", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("X", ogr.OFTReal))
    ds = None

    ds = ogr.Open('tmp/ogr_vrt_33', update = 1)
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
            # Minimalistic definition
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
                if lyr.GetGeomType() != ogr.wkbPoint:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif j == 1:
                if lyr.GetSpatialRef().ExportToWkt().find('GEOGCS') != 0:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif j == 2:
                if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() != ogr.wkbPolygon:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif j == 3:
                if lyr.GetExtent(geom_field = 1) != (0,1,0,1):
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif j == 4:
                if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('PROJCS') != 0:
                    gdaltest.post_reason('fail')
                    return 'fail'
                feat = lyr.GetNextFeature()
                if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
                    gdaltest.post_reason('fail')
                    feat.DumpReadable()
                    return 'fail'
                if feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
                    gdaltest.post_reason('fail')
                    feat.DumpReadable()
                    return 'fail'

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            f = open('tmp/ogr_vrt_33.vrt', 'wb')
            f.write(ds_str)
            f.close()
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
            os.unlink('tmp/ogr_vrt_33.vrt')

            if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
                gdaltest.post_reason('fail')
                print(ret)
                return 'fail'

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
                if lyr.GetGeomType() != ogr.wkbPolygon25D:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif i == 1:
                if lyr.GetSpatialRef().ExportToWkt().find('+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs') < 0:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif i == 2:
                if lyr.GetGeometryColumn() != 'foo':
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif j == 3:
                if lyr.TestCapability(ogr.OLCFastGetExtent) != 1:
                    gdaltest.post_reason('fail')
                    return 'fail'
                if lyr.GetExtent(geom_field = 0) != (1,2,3,4):
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif i == 4:
                if lyr.TestCapability(ogr.OLCFastFeatureCount) != 0:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif i == 5:
                if lyr.GetLayerDefn().GetGeomFieldCount() != 1:
                    gdaltest.post_reason('fail')
                    return 'fail'
                feat = lyr.GetNextFeature()
                if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0.5 1.0,1 1,1.0 0.5,0.5 0.5,0.5 1.0))':
                    gdaltest.post_reason('fail')
                    feat.DumpReadable()
                    return 'fail'

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            f = open('tmp/ogr_vrt_33.vrt', 'wb')
            f.write(ds_str)
            f.close()
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
            os.unlink('tmp/ogr_vrt_33.vrt')

            if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
                gdaltest.post_reason('fail')
                print(ret)
                return 'fail'

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
            if lyr.GetGeomType() != ogr.wkbNone:
                gdaltest.post_reason('fail')
                return 'fail'
        elif i == 1:
            if lyr.GetSpatialRef() is not None:
                gdaltest.post_reason('fail')
                return 'fail'
        elif i == 2:
            lyr.TestCapability(ogr.OLCFastFeatureCount)
            lyr.TestCapability(ogr.OLCFastGetExtent)
            lyr.TestCapability(ogr.OLCFastSpatialFilter)
        elif i == 3:
            if lyr.GetLayerDefn().GetGeomFieldCount() != 0:
                gdaltest.post_reason('fail')
                return 'fail'
            feat = lyr.GetNextFeature()
            if feat.GetGeomFieldRef(0) is not None:
                gdaltest.post_reason('fail')
                feat.DumpReadable()
                return 'fail'

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (-1 -2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    lyr.SetSpatialFilterRect( 1, 0, 0, 0, 0)
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilterRect( 1, -1.1, -2.1, -0.9, -1.9)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilter(1, None)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetIgnoredFields( ['geom__WKT_EPSG_4326_POINT'] )
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0) is not None or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (-1 -2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    lyr.SetIgnoredFields( ['foo'] )
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(1) is not None or \
       feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    if lyr.GetSpatialRef().ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt().find('POINT') < 0 or \
       feat.GetGeomFieldRef(0).ExportToWkt() == 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

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
    if lyr.GetSpatialRef().ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt().find('POINT') < 0 or \
       feat.GetGeomFieldRef(0).ExportToWkt() == 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

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
    ds = ogr.Open(ds_str)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef().ExportToWkt().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(1).ExportToWkt().find('POLYGON') < 0 or \
       feat.GetGeomFieldRef(1).ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

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
    geom_fields = [ ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326' ],
                    ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632' ],
                    ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326' ],
                    ['geom__WKT_EPSG_32631_POINT', ogr.wkbPoint, '32631' ] ]
    if lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(geom_fields)):
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3) is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3).ExportToWkt() != 'POINT (5 6)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    geom_fields = [ ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326' ],
                    ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632' ] ]
    if lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(geom_fields)):
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    geom_fields = [ ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326' ],
                    ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632' ],
                    ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326' ] ]
    if lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(geom_fields)):
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    geom_fields = [ ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632' ],
                    ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326' ] ]
    if lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(geom_fields)):
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            print(i)
            print(lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType())
            print(geom_fields[i][1])
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    geom_fields = [ ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon25D, '32632' ],
                    ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4322' ] ]
    if lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    bb = lyr.GetExtent(geom_field = 1)
    if bb != ( 1, 3, 2, 4 ):
        gdaltest.post_reason('fail')
        print(bb)
        return 'fail'
    for i in range(len(geom_fields)):
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            print(i)
            print(lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType())
            print(geom_fields[i][1])
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(1).ExportToWkt().find('POINT (') != 0 or \
       feat.GetGeomFieldRef(1).ExportToWkt() == 'POINT (1 2)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

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
    if lyr.GetLayerDefn().GetGeomFieldCount() != 0:
        print(lyr.GetLayerDefn().GetGeomFieldCount())
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldCount() != 6:
        print(lyr.GetLayerDefn().GetGeomFieldCount())
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is not None:
        f = open('tmp/ogr_vrt_33.vrt', 'wb')
        f.write(ds_str)
        f.close()
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_vrt_33.vrt')
        os.unlink('tmp/ogr_vrt_33.vrt')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

    ds = ogr.Open('tmp/ogr_vrt_33')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test UNION ALL SELECT * FROM test2')
    geom_fields = [ ['geom__WKT_EPSG_4326_POINT', ogr.wkbPoint, '4326' ],
                    ['geom__WKT_EPSG_32632_POLYGON', ogr.wkbPolygon, '32632' ],
                    ['geom__WKT_EPSG_4326_LINESTRING', ogr.wkbLineString, '4326' ],
                    ['geom__WKT_EPSG_32631_POINT', ogr.wkbPoint, '32631' ] ]
    if sql_lyr.GetLayerDefn().GetGeomFieldCount() != len(geom_fields):
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(len(geom_fields)):
        if sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName() != geom_fields[i][0]:
            gdaltest.post_reason('fail')
            return 'fail'
        if sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetType() != geom_fields[i][1]:
            gdaltest.post_reason('fail')
            return 'fail'
        if sql_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetSpatialRef().ExportToWkt().find(geom_fields[i][2]) < 0:
            gdaltest.post_reason('fail')
            return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3) is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef(1).ExportToWkt() != 'POLYGON ((1 1,1 2,2 2,2 1,1 1))' or \
       feat.GetGeomFieldRef(2) is not None or \
       feat.GetGeomFieldRef(3).ExportToWkt() != 'POINT (5 6)':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    return 'success'

###############################################################################
# Test SetIgnoredFields() with with PointFromColumns geometries

def ogr_vrt_34():
    if gdaltest.vrt_ds is None:
        return 'skip'

    f = open('tmp/test.csv', 'wb')
    f.write('x,y\n'.encode('ascii'))
    f.write('2,49\n'.encode('ascii'))
    f.close()

    try:
        os.remove('tmp/test.csvt')
    except:
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

    ds = ogr.Open( vrt_xml )
    lyr = ds.GetLayerByName( 'test' )
    lyr.SetIgnoredFields(['x', 'y'])
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        return 'fail'
    ds = None

    os.unlink('tmp/test.csv')

    return 'success'

###############################################################################
# 

def ogr_vrt_cleanup():

    if gdaltest.vrt_ds is None:
        return 'skip'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')
    
    try:
        os.unlink('tmp/ogr_vrt_33.vrt')
    except:
        pass

    try:
        import shutil
        shutil.rmtree( 'tmp/ogr_vrt_33' )
    except:
        pass

    gdaltest.vrt_ds.Destroy()
    gdaltest.vrt_ds = None

    return 'success'

gdaltest_list = [
    ogr_vrt_1,
    ogr_vrt_2,
    ogr_vrt_3,
    ogr_vrt_4,
    ogr_vrt_5,
    ogr_vrt_6,
    ogr_vrt_7,
    ogr_vrt_8,
    ogr_vrt_9,
    ogr_vrt_10,
    ogr_vrt_11,
    ogr_vrt_12,
    ogr_vrt_13,
    ogr_vrt_14,
    ogr_vrt_15,
    ogr_vrt_16,
    ogr_vrt_17,
    ogr_vrt_18,
    ogr_vrt_19_optimized,
    ogr_vrt_19_nonoptimized,
    ogr_vrt_20,
    ogr_vrt_21,
    ogr_vrt_22,
    ogr_vrt_23,
    ogr_vrt_24,
    ogr_vrt_25,
    ogr_vrt_26,
    ogr_vrt_27,
    ogr_vrt_28,
    ogr_vrt_29,
    ogr_vrt_30,
    ogr_vrt_31,
    ogr_vrt_32,
    ogr_vrt_33,
    ogr_vrt_34,
    ogr_vrt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_vrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


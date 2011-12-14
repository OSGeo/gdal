#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR VRT driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import ogr
import gdal

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
    f.write('x,val1,y,val2\n'.encode('ascii'))
    f.write('2,"val11",49,"val12"\n'.encode('ascii'))
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
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open( vrt_xml, update = 1 )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    # Only val1 and val2 attributes should be reported
    if vrt_lyr.GetLayerDefn().GetFieldCount() != 2:
        return 'fail'
    if vrt_lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() != 'val1':
        return 'fail'
    if vrt_lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef() != 'val2':
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
        return 'fail'
    if feat.GetFieldAsString('val1') != 'val21':
        return 'fail'
    feat.Destroy()

    # The x and y fields are considered as string by default, so spatial
    # filter cannot be turned into attribute filter
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    vrt_lyr.SetSpatialFilterRect(0, 40, 10, 49.5)
    ret = vrt_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find('not declared as numeric fields') == -1:
        return 'fail'
    if ret != 1:
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
        return 'fail'
    if gdal.GetLastErrorMsg() != '':
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
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
        <SrcRegion>POLYGON((0 40,0 50,10 50,10 40,0 40))</SrcRegion>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

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
    </OGRVRTLayer>
</OGRVRTDataSource>"""
        
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

    if vrt_lyr.GetLayerDefn().GetFieldCount() != 3:
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
# Run test_ogrsf

def ogr_vrt_19():

    if gdaltest.vrt_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/poly_vrt.vrt')

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
    <OGRVRTLayer name="test">
        <SrcDataSource relativeToVRT="0">tmp/test.shp</SrcDataSource>
        <SrcLayer>test</SrcLayer>
    </OGRVRTLayer>
</OGRVRTDataSource>"""
    vrt_ds = ogr.Open( vrt_xml )
    vrt_lyr = vrt_ds.GetLayerByName( 'test' )

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
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetSpatialRef().ExportToWkt().find('84') == -1:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.ResetReading()
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetNextFeature() is None:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFeature(1) is None:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetFeatureCount() == 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.SetNextByIndex(1) != 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetLayerDefn().GetFieldCount() == 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.SetAttributeFilter('') != 0:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    lyr.SetSpatialFilter(None)
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        return 'fail'
    ds = None

    ds = ogr.Open('data/vrt_test.vrt')
    lyr = ds.GetLayerByName('test3')
    if lyr.GetExtent() is None:
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
# 

def ogr_vrt_cleanup():

    if gdaltest.vrt_ds is None:
        return 'skip'

    gdal.Unlink('/vsimem/rec1.vrt')
    gdal.Unlink('/vsimem/rec2.vrt')

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
    ogr_vrt_19,
    ogr_vrt_20,
    ogr_vrt_21,
    ogr_vrt_22,
    ogr_vrt_23,
    ogr_vrt_24,
    ogr_vrt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_vrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


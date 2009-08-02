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
        gdaltest.post_reason( 'attribute filter not passed to sublayer.' )

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
    f.write('x,val1,y,val2\n')
    f.write('2,"val11",49,"val12"\n')
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
    f.write('Real,String,Real,String\n')
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
    f.write('wkt_geom,val1,val2\n')
    f.write('POINT (2 49),"val11","val12"\n')
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
    f.write('wkb_geom,val1,val2\n')
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
# 

def ogr_vrt_cleanup():

    if gdaltest.vrt_ds is None:
        return 'skip'

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
    ogr_vrt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_vrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


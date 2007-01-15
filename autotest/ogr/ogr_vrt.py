#!/usr/bin/env python
###############################################################################
# $Id: ogr_vrt.py,v 1.8 2006/03/29 18:14:40 fwarmerdam Exp $
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
# 
#  $Log: ogr_vrt.py,v $
#  Revision 1.8  2006/03/29 18:14:40  fwarmerdam
#  Use release, not destroy for shared datasource.
#
#  Revision 1.7  2006/03/29 18:09:34  fwarmerdam
#  Make sure we release sub_ds.
#
#  Revision 1.6  2005/08/02 20:16:49  fwarmerdam
#  added a SetAttributeFilter test to make sure passed to sublayer
#
#  Revision 1.5  2005/03/01 21:59:29  fwarmerdam
#  Use SetSpatialFilterRect().
#
#  Revision 1.4  2004/01/09 18:56:36  warmerda
#  Added VRT in filename test
#
#  Revision 1.3  2003/12/30 18:53:50  warmerda
#  added new SrcSQL test
#
#  Revision 1.2  2003/11/07 21:53:46  warmerda
#  added GetFeature tests
#
#  Revision 1.1  2003/11/07 21:08:31  warmerda
#  New
#
#

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
    ogr_vrt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_vrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


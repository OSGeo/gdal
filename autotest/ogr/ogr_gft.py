#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GFT driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import gdal
import ogr

###############################################################################
# Test if driver is available

def ogr_gft_init():

    ogrtest.gft_drv = None

    try:
        ogrtest.gft_drv = ogr.GetDriverByName('GFT')
    except:
        pass

    if ogrtest.gft_drv is None:
        return 'skip'

    if gdaltest.gdalurlopen('http://www.google.com') is None:
        print('cannot open http://www.google.com')
        ogrtest.gft_drv = None
        return 'skip'

    ogrtest.gft_refresh = '1/woRTxgfN8dLUpRhnOL-BXG-f7VxzXKy_3D5eizH8bS8'

    return 'success'

###############################################################################
# Read test on Wikileaks Afgan War Diary 2004-2010 table.

def ogr_gft_read():
    if ogrtest.gft_drv is None:
        return 'skip'

    table_id = '15eIgEVTUNlC649ODphJVHXeb4nsOb49WJUlnhw'

    old_auth = gdal.GetConfigOption('GFT_AUTH', None)
    old_access = gdal.GetConfigOption('GFT_ACCESS_TOKEN', None)
    old_refresh = gdal.GetConfigOption('GFT_REFRESH_TOKEN', None)
    gdal.SetConfigOption('GFT_AUTH', None)
    gdal.SetConfigOption('GFT_ACCESS_TOKEN', None)
    gdal.SetConfigOption('GFT_REFRESH_TOKEN', None)
    ds = ogr.Open('GFT:tables='+table_id)
    gdal.SetConfigOption('GFT_AUTH', old_auth)
    gdal.SetConfigOption('GFT_ACCESS_TOKEN', old_access)
    gdal.SetConfigOption('GFT_REFRESH_TOKEN', old_refresh)
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        return 'fail'

    lyr.SetSpatialFilterRect(67,31.5,67.5,32)
    lyr.SetAttributeFilter("'Attack on' = 'ENEMY'")

    count = lyr.GetFeatureCount()
    if count == 0:
        gdaltest.post_reason('did not get expected feature count')
        print(count)
        if gdaltest.skip_on_travis():
            ogrtest.gft_drv = None
            return 'skip'
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT Latitude, Longitude FROM "+table_id+" WHERE ST_INTERSECTS('Latitude', RECTANGLE(LATLNG(31.5,67.0), LATLNG(32.0,67.5))) AND 'Attack on' = 'ENEMY'")
    if sql_lyr is None:
        gdaltest.post_reason('SQL request failed')
        return 'fail'
    sql_lyr_count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    if sql_lyr_count != count:
        gdaltest.post_reason('did not get expected feature count. Got %d, expected %d' % (sql_lyr_count, count))
        return 'fail'

    return 'success'

###############################################################################
# Write test

def ogr_gft_write():
    if ogrtest.gft_drv is None:
        return 'skip'

    if ogrtest.gft_refresh is None:
        ogrtest.gft_can_write = False
        return 'skip'

    ds = ogr.Open('GFT:refresh=%s' % ogrtest.gft_refresh, update = 1)
    if ds is None:
        ogrtest.gft_can_write = False
        return 'skip'
    ogrtest.gft_can_write = True

    import random
    ogrtest.gft_rand_val = random.randint(0,2147000000)
    table_name = "test_%d" % ogrtest.gft_rand_val

    lyr = ds.CreateLayer(table_name)
    lyr.CreateField(ogr.FieldDefn('strcol', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('numcol', ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())

    feat.SetField('strcol', 'foo')
    feat.SetField('numcol', '3.45')
    expected_wkt = "POLYGON ((0 0,0 1,1 1,1 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25))"
    geom = ogr.CreateGeometryFromWkt(expected_wkt)
    feat.SetGeometry(geom)
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('CreateFeature() failed')
        return 'fail'

    fid = feat.GetFID()
    feat.SetField('strcol', 'bar')
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('SetFeature() failed')
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('strcol') != 'bar':
        gdaltest.post_reason('GetNextFeature() did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetFeature(fid)
    if feat.GetFieldAsString('strcol') != 'bar':
        gdaltest.post_reason('GetFeature() did not get expected feature')
        feat.DumpReadable()
        return 'fail'
    got_wkt = feat.GetGeometryRef().ExportToWkt()
    if got_wkt != expected_wkt:
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('GetFeatureCount() did not returned expected value')
        return 'fail'

    if lyr.DeleteFeature(feat.GetFID()) != 0:
        gdaltest.post_reason('DeleteFeature() failed')
        return 'fail'

    ds.ExecuteSQL('DELLAYER:%s' % table_name)

    ds = None

    return 'success'

###############################################################################
# ogr2ogr test to create a non-spatial GFT table

def ogr_gft_ogr2ogr_non_spatial():
    if ogrtest.gft_drv is None:
        return 'skip'

    if not ogrtest.gft_can_write:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    layer_name = 'no_geometry_table_%d' % ogrtest.gft_rand_val

    f = open('tmp/no_geometry_table.csv', 'wt')
    f.write('foo,bar\n')
    f.write('"baz","foo"\n')
    f.write('"baz2","foo2"\n')
    f.write('"baz\'3","foo3"\n')
    f.close()
    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GFT "GFT:refresh=' + ogrtest.gft_refresh + '" tmp/no_geometry_table.csv -nln ' + layer_name + ' -overwrite')

    os.unlink('tmp/no_geometry_table.csv')

    ds = ogr.Open('GFT:refresh=%s' % ogrtest.gft_refresh, update = 1)
    lyr = ds.GetLayerByName(layer_name)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('did not get expected field count')
        ds.ExecuteSQL('DELLAYER:' + layer_name)
        return 'fail'

    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('did not get expected layer geometry type')
        ds.ExecuteSQL('DELLAYER:' + layer_name)
        return 'fail'

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason('did not get expected feature count')
        ds.ExecuteSQL('DELLAYER:' + layer_name)
        return 'fail'

    ds.ExecuteSQL('DELLAYER:' + layer_name)

    ds = None

    return 'success'

###############################################################################
# ogr2ogr test to create a spatial GFT table

def ogr_gft_ogr2ogr_spatial():
    if ogrtest.gft_drv is None:
        return 'skip'

    if not ogrtest.gft_can_write:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    layer_name = 'geometry_table_%d' % ogrtest.gft_rand_val
    copied_layer_name = 'copied_geometry_table_%d' % ogrtest.gft_rand_val

    f = open('tmp/geometry_table.csv', 'wt')
    f.write('foo,bar,WKT\n')
    f.write('"baz",2,"POINT (0 1)"\n')
    f.write('"baz2",4,"POINT (2 3)"\n')
    f.write('"baz\'3",6,"POINT (4 5)"\n')
    f.close()
    f = open('tmp/geometry_table.csvt', 'wt')
    f.write('String,Integer,String\n')
    f.close()

    # Create a first table
    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GFT "GFT:refresh=' + ogrtest.gft_refresh + '" tmp/geometry_table.csv -nln ' + layer_name + ' -select foo,bar -overwrite')

    # Test round-tripping
    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GFT "GFT:refresh=' + ogrtest.gft_refresh + '" "GFT:refresh=' + ogrtest.gft_refresh + '" ' + layer_name + ' -nln ' + copied_layer_name + ' -overwrite')

    os.unlink('tmp/geometry_table.csv')
    os.unlink('tmp/geometry_table.csvt')

    ds = ogr.Open('GFT:refresh=%s' % ogrtest.gft_refresh, update = 1)

    for name in [layer_name, copied_layer_name]:
        lyr = ds.GetLayerByName(name)

        if lyr.GetGeometryColumn() != 'geometry':
            gdaltest.post_reason('layer %s: did not get expected geometry column' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

        if lyr.GetLayerDefn().GetFieldCount() != 3:
            gdaltest.post_reason('layer %s: did not get expected field count' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

        if lyr.GetGeomType() != ogr.wkbUnknown:
            gdaltest.post_reason('layer %s: did not get expected layer geometry type' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

        if lyr.GetFeatureCount() != 3:
            gdaltest.post_reason('layer %s: did not get expected feature count' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != "POINT (0 1)":
            gdaltest.post_reason('layer %s: did not get expected geometry' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

        if feat.GetFieldAsInteger('bar') != 2:
            gdaltest.post_reason('layer %s: did not get expected field value' % name)
            ds.ExecuteSQL('DELLAYER:' + layer_name)
            ds.ExecuteSQL('DELLAYER:' + copied_layer_name)
            return 'fail'

    ds.ExecuteSQL('DELLAYER:' + layer_name)
    ds.ExecuteSQL('DELLAYER:' + copied_layer_name)

    ds = None

    return 'success'

gdaltest_list = [ 
    ogr_gft_init,
    ogr_gft_read,
    ogr_gft_write,
    ogr_gft_ogr2ogr_non_spatial,
    ogr_gft_ogr2ogr_spatial,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gft' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

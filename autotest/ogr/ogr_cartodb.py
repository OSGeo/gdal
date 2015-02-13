#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CartoDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import uuid

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test if driver is available

def ogr_cartodb_init():

    ogrtest.cartodb_drv = None

    try:
        ogrtest.cartodb_drv = ogr.GetDriverByName('CartoDB')
    except:
        pass

    if ogrtest.cartodb_drv is None:
        return 'skip'

    ogrtest.cartodb_test_server = 'https://gdalautotest2.cartodb.com'
   
    if gdaltest.gdalurlopen(ogrtest.cartodb_test_server) is None:
        print('cannot open %s' % ogrtest.cartodb_test_server)
        ogrtest.cartodb_drv = None
        return 'skip'

    return 'success'

###############################################################################
#  Run test_ogrsf

def ogr_cartodb_test_ogrsf():
    if ogrtest.cartodb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config CARTODB_HTTPS NO -ro "CARTODB:gdalautotest2 tables=tm_world_borders_simpl_0_3"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_cartodb_init,
    ogr_cartodb_test_ogrsf
    ]




###############################################################################
# Test if driver is available

def ogr_cartodb_rw_init():

    ogrtest.cartodb_drv = None
    
    ogrtest.cartodb_connection = gdal.GetConfigOption('CARTODB_CONNECTION')
    if ogrtest.cartodb_connection is None:
        print('CARTODB_CONNECTION missing')
        return 'skip'
    if gdal.GetConfigOption('CARTODB_API_KEY') is None:
        print('CARTODB_API_KEY missing')
        return 'skip'

    try:
        ogrtest.cartodb_drv = ogr.GetDriverByName('CartoDB')
    except:
        pass

    if ogrtest.cartodb_drv is None:
        return 'skip'

    return 'success'

###############################################################################
# Read/write/update test

def ogr_cartodb_rw_1():

    if ogrtest.cartodb_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    if ds is None:
        return 'fail'

    lyr_name = "layer_" + str(uuid.uuid1())

    # No-op
    lyr = ds.CreateLayer(lyr_name)
    ds.DeleteLayer(ds.GetLayerCount()-1)

    # Differed table creation
    lyr = ds.CreateLayer(lyr_name)
    lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("doublefield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    fd = ogr.FieldDefn("bool", ogr.OFTInteger)
    fd.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fd)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.StartTransaction()
    lyr.CreateFeature(f)
    f.SetField('strfield', "fo'o")
    f.SetField('intfield', 123)
    f.SetField('int64field', 12345678901234)
    f.SetField('doublefield', 1.23)
    f.SetField('dt', '2014/12/04 12:34:56')
    f.SetField('bool', 0)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)
    lyr.CommitTransaction()
    f.SetField('intfield', 456)
    f.SetField('bool', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    lyr.SetFeature(f)
    fid = f.GetFID()
    ds = None

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    lyr = ds.GetLayerByName(lyr_name)
    f = lyr.GetFeature(fid)
    if f.GetField('strfield') != "fo'o" or \
       f.GetField('intfield') != 456 or \
       f.GetField('int64field') != 12345678901234 or \
       f.GetField('doublefield') != 1.23 or \
       f.GetField('dt') != '2014/12/04 12:34:56+00' or \
       f.GetField('bool') != 1 or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'

    lyr.DeleteFeature(fid)
    f = lyr.GetFeature(fid)
    if f is not None:
        gdaltest.post_reason('fail')
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'

    # Non-differed field creation
    lyr.CreateField(ogr.FieldDefn("otherstrfield", ogr.OFTString))

    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(lyr_name, geom_type = ogr.wkbMultiPolygon, srs = srs)
    lyr.GetNextFeature()
    ds.ExecuteSQL("DELLAYER:" + lyr_name)
    
    # Layer without geometry
    lyr = ds.CreateLayer(lyr_name, geom_type = ogr.wkbNone)
    fd = ogr.FieldDefn("nullable", ogr.OFTString)
    lyr.CreateField(fd)
    fd = ogr.FieldDefn("not_nullable", ogr.OFTString)
    fd.SetNullable(0)
    lyr.CreateField(fd)

    field_defn = ogr.FieldDefn( 'field_string', ogr.OFTString )
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)
    
    field_defn = ogr.FieldDefn( 'field_datetime_with_default', ogr.OFTDateTime )
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)
    
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('not_nullable', 'foo')
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    lyr = ds.GetLayerByName(lyr_name)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'a''b'":
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime_with_default')).GetDefault() != 'CURRENT_TIMESTAMP':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None or f.GetField('field_string') != 'a\'b' or not f.IsFieldSet('field_datetime_with_default'):
        gdaltest.post_reason('fail')
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'
    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    return 'success'

gdaltest_rw_list = [
    ogr_cartodb_rw_init,
    ogr_cartodb_rw_1,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cartodb' )

    if gdal.GetConfigOption('CARTODB_CONNECTION') is None:
        print('CARTODB_CONNECTION missing: running read-only tests')
        gdaltest.run_tests( gdaltest_list )
    else:
        gdaltest.run_tests( gdaltest_rw_list )

    gdaltest.summarize()

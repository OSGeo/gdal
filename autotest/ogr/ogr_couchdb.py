#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CouchDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import uuid

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Test if driver is available

def ogr_couchdb_init():

    ogrtest.couchdb_drv = None

    try:
        ogrtest.couchdb_drv = ogr.GetDriverByName('CouchDB')
    except:
        pass

    if ogrtest.couchdb_drv is None:
        return 'skip'

    if 'COUCHDB_TEST_SERVER' in os.environ:
        ogrtest.couchdb_test_server = os.environ['COUCHDB_TEST_SERVER']
    else:
        ogrtest.couchdb_test_server = 'http://127.0.0.1:5984'
    ogrtest.couchdb_temp_layer_name = 'layer_' + str(uuid.uuid1()).replace('-', '_')

    if gdaltest.gdalurlopen(ogrtest.couchdb_test_server) is None:
        print('cannot open %s' % ogrtest.couchdb_test_server)
        ogrtest.couchdb_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Basic test

def ogr_couchdb_1():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    gdal.VectorTranslate('CouchDB:' + ogrtest.couchdb_test_server, 'data/poly.shp',
                         format = 'CouchDB',
                         layerName = ogrtest.couchdb_temp_layer_name,
                         layerCreationOptions = ['UPDATE_PERMISSIONS=ALL'])
    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update = 1)
    if ds is None:
        return 'fail'
    lyr = ds.GetLayerByName(ogrtest.couchdb_temp_layer_name)
    f = lyr.GetNextFeature()
    if f['AREA'] != 215229.266 or f['EAS_ID'] != '168' or f.GetGeometryRef() is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ExecuteSQL('DELLAYER:' + ogrtest.couchdb_temp_layer_name)

    return 'success'

###############################################################################
# Test null / unset

def ogr_couchdb_2():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update = 1)
    if ds is None:
        return 'fail'
    lyr = ds.CreateLayer(ogrtest.couchdb_temp_layer_name, geom_type = ogr.wkbNone, options = ['UPDATE_PERMISSIONS=ALL'])
    lyr.CreateField( ogr.FieldDefn('str_field', ogr.OFTString) )

    f = ogr.Feature(lyr.GetLayerDefn())
    f['str_field'] = 'foo'
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('str_field')
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f['str_field'] != None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f.IsFieldSet('str_field'):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'


###############################################################################
# Cleanup

def ogr_couchdb_cleanup():
    if ogrtest.couchdb_drv is None:
        return 'skip'

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update = 1)
    if ds is None:
        return 'fail'
    ds.ExecuteSQL('DELLAYER:' + ogrtest.couchdb_temp_layer_name)

    return 'success'

gdaltest_list = [
    ogr_couchdb_init,
    ogr_couchdb_1,
    ogr_couchdb_2,
    ogr_couchdb_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_couchdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PGEO driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
import ogr
import osr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Basic testing

def ogr_pgeo_1(tested_driver = 'PGeo', other_driver = 'MDB'):

    ogrtest.pgeo_ds = None

    try:
        ogrtest.other_driver = ogr.GetDriverByName(other_driver)
    except:
        ogrtest.other_driver = None
    if ogrtest.other_driver is not None:
        print('Unregistering %s driver' % ogrtest.other_driver.GetName())
        ogrtest.other_driver.Deregister()
        if other_driver == 'PGeo':
            # Re-register Geomedia and WALK at the end, *after* MDB
            geomedia_driver = ogr.GetDriverByName('Geomedia')
            if geomedia_driver is not None:
                geomedia_driver.Deregister()
                geomedia_driver.Register()
            walk_driver = ogr.GetDriverByName('WALK')
            if walk_driver is not None:
                walk_driver.Deregister()
                walk_driver.Register()

    try:
        drv = ogr.GetDriverByName(tested_driver)
    except:
        drv = None

    if drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip', 'PGeoTest.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/Autodesk Test.mdb')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/PGeoTest.zip')
            try:
                os.stat('tmp/cache/Autodesk Test.mdb')
            except:
                return 'skip'
        except:
            return 'skip'
            
    ogrtest.pgeo_ds = ogr.Open('tmp/cache/Autodesk Test.mdb')
    if ogrtest.pgeo_ds is None:
        gdaltest.post_reason('could not open DB. Driver probably misconfigured')
        return 'skip'
    
    if ogrtest.pgeo_ds.GetLayerCount() != 3:
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'
        
    lyr = ogrtest.pgeo_ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        gdaltest.post_reason('did not get expected attributes')
        feat.DumpReadable()
        return 'fail'
        
    if ogrtest.check_feature_geometry(feat,'LINESTRING (1910941.703951031 445833.57942859828 0,1910947.927691862 445786.43811868131 0)', max_error = 0.0000001) != 0:
        gdaltest.post_reason('did not get expected geometry')
        feat.DumpReadable()
        return 'fail'

    feat_count = lyr.GetFeatureCount()
    if feat_count != 9418:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    return 'success'

def ogr_pgeo_mdb_1():
    return ogr_pgeo_1('MDB', 'PGeo')

###############################################################################
# Test spatial filter

def ogr_pgeo_2():
    if ogrtest.pgeo_ds is None:
        return 'skip'
        
    lyr = ogrtest.pgeo_ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    bbox = geom.GetEnvelope()
    
    lyr.SetSpatialFilterRect(bbox[0], bbox[1], bbox[2], bbox[3])

    feat_count = lyr.GetFeatureCount()
    if feat_count != 6957:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        gdaltest.post_reason('did not get expected attributes')
        feat.DumpReadable()
        return 'fail'
        
    # Check that geometry filter is well cleared
    lyr.SetSpatialFilter(None)
    feat_count = lyr.GetFeatureCount()
    if feat_count != 9418:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'
        
    return 'success'

###############################################################################
# Test attribute filter

def ogr_pgeo_3():
    if ogrtest.pgeo_ds is None:
        return 'skip'
        
    lyr = ogrtest.pgeo_ds.GetLayer(0)
    lyr.SetAttributeFilter('OBJECTID=1')

    feat_count = lyr.GetFeatureCount()
    if feat_count != 1:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        gdaltest.post_reason('did not get expected attributes')
        feat.DumpReadable()
        return 'fail'
        
    # Check that attribute filter is well cleared (#3706)
    lyr.SetAttributeFilter(None)
    feat_count = lyr.GetFeatureCount()
    if feat_count != 9418:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    return 'success'

###############################################################################
# Test ExecuteSQL()

def ogr_pgeo_4():
    if ogrtest.pgeo_ds is None:
        return 'skip'
        
    sql_lyr = ogrtest.pgeo_ds.ExecuteSQL('SELECT * FROM SDPipes WHERE OBJECTID = 1')

    feat_count = sql_lyr.GetFeatureCount()
    if feat_count != 1:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        gdaltest.post_reason('did not get expected attributes')
        feat.DumpReadable()
        return 'fail'
        
    ogrtest.pgeo_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test GetFeature()

def ogr_pgeo_5():
    if ogrtest.pgeo_ds is None:
        return 'skip'
        
    lyr = ogrtest.pgeo_ds.GetLayer(0)
    feat = lyr.GetFeature(9418)
    if feat.GetField('OBJECTID') != 9418:
        gdaltest.post_reason('did not get expected attributes')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_pgeo_6():
    if ogrtest.pgeo_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "tmp/cache/Autodesk Test.mdb"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf with -sql

def ogr_pgeo_7():
    if ogrtest.pgeo_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "tmp/cache/Autodesk Test.mdb" -sql "SELECT * FROM SDPipes"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'
    
###############################################################################

def ogr_pgeo_cleanup():

    if ogrtest.other_driver is not None:
        print('Reregistering %s driver' % ogrtest.other_driver.GetName())
        ogrtest.other_driver.Register()

    if ogrtest.pgeo_ds is None:
        return 'skip'

    ogrtest.pgeo_ds = None
    return 'success'

gdaltest_list_internal = [
    ogr_pgeo_2,
    ogr_pgeo_3,
    ogr_pgeo_4,
    ogr_pgeo_5,
    ogr_pgeo_6,
    ogr_pgeo_7,
    ogr_pgeo_cleanup    ]

###############################################################################
#

def ogr_pgeo_main():

    # Run with the PGeo driver only (MDB disabled)
    gdaltest.run_tests( [ ogr_pgeo_1 ] )
    gdaltest.run_tests( gdaltest_list_internal )

    # Run with the MDB driver only (PGeo disabled)
    gdaltest.run_tests( [ ogr_pgeo_mdb_1 ] )
    gdaltest.run_tests( gdaltest_list_internal )

    return 'success'

gdaltest_list = [
    ogr_pgeo_main
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pgeo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WFS driver testing.
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal
import socket

###############################################################################
# Test underlying OGR drivers
#

def ogr_wfs_1():

    gdaltest.wfs_drv = None

    try:
        gdaltest.wfs_drv = ogr.GetDriverByName('WFS')
    except:
        pass
        
    if gdaltest.wfs_drv is None:
        return 'skip'
    
    gdaltest.have_gml_reader = 0

    try:
        gml_ds = ogr.Open( 'data/ionic_wfs.gml' )
    except:
        gml_ds = None

    if gml_ds is None:
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            return 'skip'
        else:
            gdaltest.post_reason( 'failed to open test file.' )
            return 'fail'

    gdaltest.have_gml_reader = 1

    return 'success'

###############################################################################
# Test reading a MapServer WFS server

def ogr_wfs_2():
    if gdaltest.wfs_drv is None:
        return 'skip'
    if not gdaltest.have_gml_reader:
        return 'skip'

    timeout =  10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen('http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap') is None:
        print('cannot open URL')
        return 'skip'

    ds = ogr.Open('WFS:http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap')
    if ds is None:
        gdaltest.post_reason('did not managed to open WFS datastore')
        return 'fail'

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'park':
        gdaltest.post_reason('did not get expected layer name')
        print(lyr.GetName())
        return 'fail'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(42304)
    if not sr.IsSame(sr2):
        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        return 'fail'

    feat_count = lyr.GetFeatureCount()
    if feat_count != 46:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if geom_wkt.find("POLYGON ((389366.84375 3791519.75") == -1:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'


###############################################################################
# Test reading a GeoServer WFS server

def ogr_wfs_3():
    if gdaltest.wfs_drv is None:
        return 'skip'
    if not gdaltest.have_gml_reader:
        return 'skip'

    timeout =  10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen('http://sigma.openplans.org/geoserver/ows') is None:
        print('cannot open URL')
        return 'skip'

    ds = ogr.Open('WFS:http://sigma.openplans.org/geoserver/ows?TYPENAME=za:za_points')
    if ds is None:
        gdaltest.post_reason('did not managed to open WFS datastore')
        return 'fail'

    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'za:za_points':
        gdaltest.post_reason('did not get expected layer name')
        print(lyr.GetName())
        return 'fail'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(4326)
    if not sr.IsSame(sr2):
        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        return 'fail'

    feat_count = lyr.GetFeatureCount()
    if feat_count != 14236:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    if not lyr.TestCapability(ogr.OLCFastFeatureCount):
        gdaltest.post_reason('did not get OLCFastFeatureCount')
        return 'fail'

    ds = ogr.Open('WFS:http://sigma.openplans.org/geoserver/ows?TYPENAME=za:za_points&MAXFEATURES=10')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if feat.GetField('name') != 'Alexander Bay' or \
       geom_wkt.find("POINT (16.4827778 -28.5947222)") == -1:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    # Same with VERSION=1.0.0
    ds = ogr.Open('WFS:http://sigma.openplans.org/geoserver/ows?TYPENAME=za:za_points&MAXFEATURES=10&VERSION=1.0.0')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if feat.GetField('name') != 'Alexander Bay' or \
       geom_wkt.find("POINT (16.4827778 -28.5947222)") == -1:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test reading a GeoServer WFS server with OUTPUTFORMAT=json

def ogr_wfs_4():
    if gdaltest.wfs_drv is None:
        return 'skip'

    timeout =  10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen('http://sigma.openplans.org/geoserver/ows') is None:
        print('cannot open URL')
        return 'skip'

    ds = ogr.Open('WFS:http://sigma.openplans.org/geoserver/ows?TYPENAME=za:za_points&MAXFEATURES=10&OUTPUTFORMAT=json')
    if ds is None:
        gdaltest.post_reason('did not managed to open WFS datastore')
        return 'fail'

    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('did not get expected layer count')
        print(ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'za:za_points':
        gdaltest.post_reason('did not get expected layer name')
        print(lyr.GetName())
        return 'fail'

    feat_count = lyr.GetFeatureCount()
    if feat_count != 10:
        gdaltest.post_reason('did not get expected feature count')
        print(feat_count)
        return 'fail'

    if not lyr.TestCapability(ogr.OLCFastFeatureCount):
        gdaltest.post_reason('did not get OLCFastFeatureCount')
        return 'fail'

    ds = ogr.Open('WFS:http://sigma.openplans.org/geoserver/ows?TYPENAME=za:za_points&MAXFEATURES=10&OUTPUTFORMAT=json')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if feat.GetField('name') != 'Alexander Bay' or \
       geom_wkt.find("POINT (16.4827778 -28.5947222)") == -1:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'


###############################################################################
# Test reading a Deegree WFS server

def ogr_wfs_5():
    if gdaltest.wfs_drv is None:
        return 'skip'
    if not gdaltest.have_gml_reader:
        return 'skip'

    timeout =  10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen('http://demo.deegree.org/deegree-wfs/services') is None:
        print('cannot open URL')
        return 'skip'

    ds = ogr.Open("WFS:http://demo.deegree.org/deegree-wfs/services?MAXFEATURES=10")
    if ds is None:
        gdaltest.post_reason('did not managed to open WFS datastore')
        return 'fail'

    lyr = ds.GetLayerByName('app:Springs')
    if lyr.GetName() != 'app:Springs':
        gdaltest.post_reason('did not get expected layer name')
        print(lyr.GetName())
        return 'fail'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(26912)
    if not sr.IsSame(sr2):
        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if feat.GetField('objectid') != 1 or \
       geom_wkt.find("POINT (558750.703125 4402882.05)") == -1:
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_wfs_6():
    if gdaltest.wfs_drv is None:
        return 'skip'
    if not gdaltest.have_gml_reader:
        return 'skip'

    timeout =  10
    socket.setdefaulttimeout(timeout)
    if gdaltest.gdalurlopen('http://demo.deegree.org/deegree-wfs/services') is None:
        print('cannot open URL')
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro "WFS:http://demo.deegree.org/deegree-wfs/services?MAXFEATURES=10" app:Springs')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_wfs_1,
    ogr_wfs_2,
    ogr_wfs_3,
    ogr_wfs_4,
    ogr_wfs_5,
    ogr_wfs_6]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wfs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


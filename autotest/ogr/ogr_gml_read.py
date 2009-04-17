#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

###############################################################################
# Test reading geometry and attribute from ionic wfs gml file.
#

def ogr_gml_1():

    gdaltest.have_gml_reader = 0

    try:
        gml_ds = ogr.Open( 'data/ionic_wfs.gml' )
    except:
        gml_ds = None

    if gml_ds is None:
        if string.find(gdal.GetLastErrorMsg(),'Xerces') != -1:
            return 'skip'
        else:
            gdaltest.post_reason( 'failed to open test file.' )
            return 'fail'

    gdaltest.have_gml_reader = 1

    if gml_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'
    
    lyr = gml_ds.GetLayerByName('GEM')
    feat = lyr.GetNextFeature()

    if feat.GetField('Name') != 'Aartselaar':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    wkt = 'POLYGON ((44038 511549,44015 511548,43994 511522,43941 511539,43844 511514,43754 511479,43685 511521,43594 511505,43619 511452,43645 511417,4363 511387,437 511346,43749 511298,43808 511229,43819 511205,4379 511185,43728 511167,43617 511175,43604 511151,43655 511125,43746 511143,43886 511154,43885 511178,43928 511186,43977 511217,4404 511223,44008 511229,44099 51131,44095 511335,44106 51135,44127 511379,44124 511435,44137 511455,44105 511467,44098 511484,44086 511499,4407 511506,44067 511535,44038 511549))'
    
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason( 'got unexpected feature.' )
        return 'fail'

    return 'success'

###############################################################################
# Do the same test somewhere without a .gfs file.

def ogr_gml_2():
    if not gdaltest.have_gml_reader:
        return 'skip'

    # copy gml file (but not .gfs file)
    open('tmp/ionic_wfs.gml','w').write(open('data/ionic_wfs.gml').read())
    
    gml_ds = ogr.Open( 'tmp/ionic_wfs.gml' )    

    if gml_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'
    
    lyr = gml_ds.GetLayerByName('GEM')
    feat = lyr.GetNextFeature()

    if feat.GetField('Name') != 'Aartselaar':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    wkt = 'POLYGON ((44038 511549,44015 511548,43994 511522,43941 511539,43844 511514,43754 511479,43685 511521,43594 511505,43619 511452,43645 511417,4363 511387,437 511346,43749 511298,43808 511229,43819 511205,4379 511185,43728 511167,43617 511175,43604 511151,43655 511125,43746 511143,43886 511154,43885 511178,43928 511186,43977 511217,4404 511223,44008 511229,44099 51131,44095 511335,44106 51135,44127 511379,44124 511435,44137 511455,44105 511467,44098 511484,44086 511499,4407 511506,44067 511535,44038 511549))'
    
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason( 'got unexpected feature.' )
        return 'fail'

    return 'success'

###############################################################################
# Similar test for RNF style line data.

def ogr_gml_3():
    if not gdaltest.have_gml_reader:
        return 'skip'

    gml_ds = ogr.Open( 'data/rnf_eg.gml' )    

    if gml_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'
    
    lyr = gml_ds.GetLayerByName('RoadSegment')
    feat = lyr.GetNextFeature()

    if feat.GetField('ngd_id') != 817792:
        gdaltest.post_reason( 'Wrong ngd_id field value' )
        return 'fail'

    if feat.GetField('type') != 'HWY':
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    wkt = 'LINESTRING (-63.500411040289066 46.240122507771368,-63.501009714909742 46.240344881690326,-63.502170462373471 46.241041855639622,-63.505862621395394 46.24195250605576,-63.506719184531178 46.242002742901576,-63.507197272602212 46.241931577811606,-63.508403092799554 46.241752283460158,-63.509946573455622 46.241745397977233)'
    
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason( 'got unexpected feature.' )
        return 'fail'

    return 'success'

###############################################################################
# Test of read GML file with UTF-8 BOM indicator.

def ogr_gml_4():
    if not gdaltest.have_gml_reader:
        return 'skip'

    gml_ds = ogr.Open( 'data/bom.gml' )    

    if gml_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'

    lyr = gml_ds.GetLayerByName('CartographicText')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong number of features' )
        return 'fail'

    # Test 1st feature
    feat = lyr.GetNextFeature()

    if feat.GetField('featureCode') != 10198:
        gdaltest.post_reason( 'Wrong featureCode field value' )
        return 'fail'

    wkt = 'POINT (347243.85 461299.5)'

    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    # Test 2nd feature
    feat = lyr.GetNextFeature()

    if feat.GetField('featureCode') != 10069:
        gdaltest.post_reason( 'Wrong featureCode field value' )
        return 'fail'

    wkt = 'POINT (347251.45 461250.85)'

    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    return 'success'


###############################################################################
# Test of read GML file that triggeered bug #2349

def ogr_gml_5():

    if not gdaltest.have_gml_reader:
        return 'skip'

    gml_ds = ogr.Open( 'data/ticket_2349_test_1.gml' )

    lyr = gml_ds.GetLayerByName('MyPolyline')

    lyr.SetAttributeFilter( 'height > 300' )

    lyr.GetNextFeature()

    return 'success'

###############################################################################
# Test of various FIDs (various prefixes and lengths) (Ticket#1017) 
def ogr_gml_6():

    if not gdaltest.have_gml_reader: 
        return 'skip' 

    files = ['test_point1', 'test_point2', 'test_point3'] 
    fids = [] 

    for filename in files: 
        fids[:] = [] 
        gml_ds = ogr.Open( 'data' + os.sep + filename + '.gml' ) 
        lyr = gml_ds.GetLayer() 
        feat = lyr.GetNextFeature() 
        while feat is not None: 
            if ( feat.GetFID() < 0 ) or ( feat.GetFID() in fids ): 
                os.remove( 'data' + os.sep + filename + '.gfs' ) 
                gdaltest.post_reason( 'Wrong FID value' ) 
                return 'fail' 
            fids.append(feat.GetFID()) 
            feat = lyr.GetNextFeature() 
        os.remove( 'data' + os.sep + filename + '.gfs' ) 

    return 'success'

###############################################################################
# Test of colon terminated prefixes for attribute values (Ticket#2493)

def ogr_gml_7():

    if not gdaltest.have_gml_reader:
        return 'skip'

    gml_ds = ogr.Open( 'data/test_point.gml' )
    lyr = gml_ds.GetLayer()
    ldefn = lyr.GetLayerDefn()

    try:
        ldefn.GetFieldDefn(0).GetFieldTypeName
    except:
        return 'skip'

    if ldefn.GetFieldDefn(0).GetFieldTypeName(ldefn.GetFieldDefn(0).GetType())\
       != 'Real':
        return 'fail'
    if ldefn.GetFieldDefn(1).GetFieldTypeName(ldefn.GetFieldDefn(1).GetType())\
       != 'Integer':
        return 'fail'
    if ldefn.GetFieldDefn(2).GetFieldTypeName(ldefn.GetFieldDefn(2).GetType())\
       != 'String':
        return 'fail'

    return 'success'

###############################################################################
# Test a GML file with some non-ASCII UTF-8 content that triggered a bug (Ticket#2948)

def ogr_gml_8():

    if not gdaltest.have_gml_reader:
        return 'skip'

    gml_ds = ogr.Open( 'data/utf8.gml' )
    lyr = gml_ds.GetLayer()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('name') != '\xc4\x80liamanu':
        return 'fail'

    gml_ds.Destroy()

    return 'success'

###############################################################################
#  Cleanup

def ogr_gml_cleanup():
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [ 
    ogr_gml_1,
    ogr_gml_2,
    ogr_gml_3,
    ogr_gml_4,
    ogr_gml_5,
    ogr_gml_6,
    ogr_gml_7,
    ogr_gml_8,
    ogr_gml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


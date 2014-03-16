#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import shutil

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
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
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
# Test also support for nested GML elements (#3680)

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

    if feat.GetField('anchorPosition') != 8:
        gdaltest.post_reason( 'Wrong anchorPosition field value' )
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

    files = ['test_point1', 'test_point2', 'test_point3', 'test_point4'] 
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

    gdal.SetConfigOption('GML_EXPOSE_FID', 'FALSE')
    gml_ds = ogr.Open( 'data/test_point.gml' )
    gdal.SetConfigOption('GML_EXPOSE_FID', None)
    lyr = gml_ds.GetLayer()
    ldefn = lyr.GetLayerDefn()

    # Test fix for #2969
    if lyr.GetFeatureCount() != 5:
        gdaltest.post_reason( 'Bad feature count' )
        return 'fail'

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
        print(feat.GetFieldAsString('name'))
        return 'fail'

    gml_ds.Destroy()

    return 'success'

###############################################################################
# Test writing invalid UTF-8 content in a GML file (ticket #2971)

def ogr_gml_9():

    if not gdaltest.have_gml_reader:
        return 'skip'

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('tmp/broken_utf8.gml')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTString))

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('test', '\x80bad')

    # Avoid the warning
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature( dst_feat )
    gdal.PopErrorHandler()

    if ret != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'

    dst_feat.Destroy()
    ds.Destroy()

    ds = ogr.Open('tmp/broken_utf8.gml')
    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()

    if feat.GetField('test') != '?bad':
        gdaltest.post_reason('Unexpected content.')
        return 'fail'

    feat.Destroy();
    ds.Destroy()

    os.remove('tmp/broken_utf8.gml')
    os.remove('tmp/broken_utf8.xsd')

    return 'success'

###############################################################################
# Test writing different data types in a GML file (ticket #2857)
# TODO: Add test for other data types as they are added to the driver.

def ogr_gml_10():

    if not gdaltest.have_gml_reader:
        return 'skip'

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('tmp/fields.gml')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('string', ogr.OFTString)
    field_defn.SetWidth(100)
    lyr.CreateField(field_defn)
    lyr.CreateField(ogr.FieldDefn('date', ogr.OFTDate))
    field_defn = ogr.FieldDefn('real', ogr.OFTReal)
    field_defn.SetWidth(4)
    field_defn.SetPrecision(2)
    lyr.CreateField(field_defn)
    lyr.CreateField(ogr.FieldDefn('float', ogr.OFTReal))
    field_defn = ogr.FieldDefn('integer', ogr.OFTInteger)
    field_defn.SetWidth(5)
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('string', 'test string of length 24')
    dst_feat.SetField('date', '2003/04/22')
    dst_feat.SetField('real', 12.34)
    dst_feat.SetField('float', 1234.5678)
    dst_feat.SetField('integer', '1234')

    ret = lyr.CreateFeature( dst_feat )

    if ret != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'

    dst_feat.Destroy()
    ds.Destroy()

    ds = ogr.Open('tmp/fields.gml')
    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()

    if feat.GetFieldDefnRef(feat.GetFieldIndex('string')).GetType() != ogr.OFTString:
        gdaltest.post_reason('String type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('string')).GetType()))
        return 'fail'
    if feat.GetFieldDefnRef(feat.GetFieldIndex('date')).GetType() != ogr.OFTString:
        gdaltest.post_reason('Date type is not reported as OFTString. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('date')).GetType()))
        return 'fail'
    if feat.GetFieldDefnRef(feat.GetFieldIndex('real')).GetType() != ogr.OFTReal:
        gdaltest.post_reason('Real type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('real')).GetType()))
        return 'fail'
    if feat.GetFieldDefnRef(feat.GetFieldIndex('float')).GetType() != ogr.OFTReal:
        gdaltest.post_reason('Float type is not reported as OFTReal. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('float')).GetType()))
        return 'fail'
    if feat.GetFieldDefnRef(feat.GetFieldIndex('integer')).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('Integer type is reported wrong. Got ' + str(feat.GetFieldDefnRef(feat.GetFieldIndex('integer')).GetType()))
        return 'fail'

    if feat.GetField('string') != 'test string of length 24':
        gdaltest.post_reason('Unexpected string content.' + feat.GetField('string') )
        return 'fail'
    if feat.GetField('date') != '2003/04/22':
        gdaltest.post_reason('Unexpected string content.' + feat.GetField('date') )
        return 'fail'
    if feat.GetFieldAsDouble('real') != 12.34:
        gdaltest.post_reason('Unexpected real content.')
        return 'fail'
    if feat.GetField('float') != 1234.5678:
        gdaltest.post_reason('Unexpected float content.')
        return 'fail'
    if feat.GetField('integer') != 1234:
        gdaltest.post_reason('Unexpected integer content.')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('string')).GetWidth() != 100:
        gdaltest.post_reason('Unexpected width of string field.')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('real')).GetWidth() != 4:
        gdaltest.post_reason('Unexpected width of real field.')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('real')).GetPrecision() != 2:
        gdaltest.post_reason('Unexpected precision of real field.')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('integer')).GetWidth() != 5:
        gdaltest.post_reason('Unexpected width of integer field.')
        return 'fail'

    feat.Destroy();
    ds.Destroy()

    os.remove('tmp/fields.gml')
    os.remove('tmp/fields.xsd')

    return 'success'

###############################################################################
# Test reading a geometry element specified with <GeometryElementPath>

def ogr_gml_11():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/testgeometryelementpath.gml').st_mtime
        gfs_mtime = os.stat('data/testgeometryelementpath.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/testgeometryelementpath.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/testgeometryelementpath.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetGeometryColumn() != 'location1container|location1':
        gdaltest.post_reason('did not get expected geometry column name')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('attrib1') != 'attrib1_value':
        gdaltest.post_reason('did not get expected value for attrib1')
        return 'fail'
    if feat.GetField('attrib2') != 'attrib2_value':
        gdaltest.post_reason('did not get expected value for attrib2')
        return 'fail'
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('did not get expected geometry')
        return 'fail'
    ds = None
    return 'success'

###############################################################################
# Test reading a virtual GML file

def ogr_gml_12():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('/vsizip/data/testgeometryelementpath.zip/testgeometryelementpath.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetGeometryColumn() != 'location1container|location1':
        gdaltest.post_reason('did not get expected geometry column name')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('attrib1') != 'attrib1_value':
        gdaltest.post_reason('did not get expected value for attrib1')
        return 'fail'
    if feat.GetField('attrib2') != 'attrib2_value':
        gdaltest.post_reason('did not get expected value for attrib2')
        return 'fail'
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('did not get expected geometry')
        return 'fail'
    ds = None
    return 'success'

###############################################################################
# Test reading GML with StringList, IntegerList and RealList fields

def ogr_gml_13():

    if not gdaltest.have_gml_reader:
        return 'skip'
    
    ds = ogr.Open('data/testlistfields.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsStringList(feat.GetFieldIndex('attrib1')) != ['value1','value2']:
        gdaltest.post_reason('did not get expected value for attrib1')
        return 'fail'
    if feat.GetField(feat.GetFieldIndex('attrib2')) != 'value3':
        gdaltest.post_reason('did not get expected value for attrib2')
        return 'fail'
    if feat.GetFieldAsIntegerList(feat.GetFieldIndex('attrib3')) != [4,5]:
        gdaltest.post_reason('did not get expected value for attrib3')
        return 'fail'
    if feat.GetFieldAsDoubleList(feat.GetFieldIndex('attrib4')) != [6.1,7.1]:
        gdaltest.post_reason('did not get expected value for attrib4')
        return 'fail'
    ds = None
    return 'success'

###############################################################################
# Test xlink resolution

def ogr_gml_14():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # We need CURL for xlink resolution, and a sign that Curl is available
    # is the availability of the WMS driver
    try:
        gdaltest.wms_drv = gdal.GetDriverByName( 'WMS' )
    except:
        gdaltest.wms_drv = None
    if gdaltest.wms_drv is None:
        return 'skip'

    if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/gml/xlink3.gml') is None:
        print('cannot open URL')
        return 'skip'

    files = [ 'xlink1.gml', 'xlink2.gml', 'expected1.gml', 'expected2.gml' ]
    for file in files:
        if not gdaltest.download_file('http://download.osgeo.org/gdal/data/gml/' + file, file ):
            return 'skip'

    gdal.SetConfigOption( 'GML_SKIP_RESOLVE_ELEMS', 'NONE' )
    gdal.SetConfigOption( 'GML_SAVE_RESOLVED_TO', 'tmp/cache/xlink1resolved.gml' )
    gml_ds = ogr.Open( 'tmp/cache/xlink1.gml' )
    gml_ds = None
    gdal.SetConfigOption( 'GML_SKIP_RESOLVE_ELEMS', 'gml:directedNode' )
    gdal.SetConfigOption( 'GML_SAVE_RESOLVED_TO', 'tmp/cache/xlink2resolved.gml' )
    gml_ds = ogr.Open( 'tmp/cache/xlink1.gml' )
    gml_ds = None
    gdal.SetConfigOption( 'GML_SKIP_RESOLVE_ELEMS', None )
    gdal.SetConfigOption( 'GML_SAVE_RESOLVED_TO', None )

    try:
        fp = open( 'tmp/cache/xlink1resolved.gml', 'r' )
        text = fp.read()
        fp.close()
        os.remove( 'tmp/cache/xlink1resolved.gml' )
        fp = open( 'tmp/cache/expected1.gml', 'r' )
        expectedtext = fp.read()
        fp.close()
    except:
        return 'fail'

    if text != expectedtext:
        print('Problem with file 1')
        return 'fail'

    try:
        fp = open( 'tmp/cache/xlink2resolved.gml', 'r' )
        text = fp.read()
        fp.close()
        os.remove( 'tmp/cache/xlink2resolved.gml' )
        fp = open( 'tmp/cache/expected2.gml', 'r' )
        expectedtext = fp.read()
        fp.close()
    except:
        return 'fail'

    if text != expectedtext:
        print('Problem with file 2')
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_gml_15():

    if not gdaltest.have_gml_reader:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test_point.gml')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Read CityGML generic attributes

def ogr_gml_16():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/citygml.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if feat.GetField('Name_') != 'aname' or \
       feat.GetField('a_int_attr') != 2 or \
       feat.GetField('a_double_attr') != 3.45:
        feat.DumpReadable()
        gdaltest.post_reason('did not get expected values')
        return 'fail'

    return 'success'

###############################################################################
# Read layer SRS for WFS 1.0.0 return

def ogr_gml_17():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/gnis_pop_100.gml')
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    if got_wkt.find('GEOGCS["WGS 84"') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(got_wkt)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POINT (2.09 34.12)':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    return 'success'

###############################################################################
# Read layer SRS for WFS 1.1.0 return

def ogr_gml_18():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/gnis_pop_110.gml')
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    if got_wkt.find('GEOGCS["WGS 84"') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(got_wkt)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POINT (2.09 34.12)':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    return 'success'

###############################################################################
# Read layer SRS for WFS 1.1.0 return, but without trying to restore
# (long, lat) order. So we should get EPSGA:4326 and (lat, long) order

def ogr_gml_19():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.remove( 'data/gnis_pop_110.gfs' )
    except:
        pass

    gdal.SetConfigOption('GML_INVERT_AXIS_ORDER_IF_LAT_LONG', 'NO')
    ds = ogr.Open('data/gnis_pop_110.gml')
    gdal.SetConfigOption('GML_INVERT_AXIS_ORDER_IF_LAT_LONG', None)

    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    if got_wkt.find('GEOGCS["WGS 84"') == -1 or \
       got_wkt.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(got_wkt)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POINT (34.12 2.09)':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test parsing a .xsd where the type definition is before its reference

def ogr_gml_20():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.remove( 'data/archsites.gfs' )
    except:
        pass

    ds = ogr.Open('data/archsites.gml')
    lyr = ds.GetLayer(0)
    ldefn = lyr.GetLayerDefn()

    try:
        ldefn.GetFieldDefn(0).GetFieldTypeName
    except:
        return 'skip'

    idx = ldefn.GetFieldIndex("gml_id")
    if idx == -1:
        gdaltest.post_reason('did not get expected column "gml_id"')
        return 'fail'

    idx = ldefn.GetFieldIndex("cat")
    fddefn = ldefn.GetFieldDefn(idx)
    if fddefn.GetFieldTypeName(fddefn.GetType()) != 'Integer':
        gdaltest.post_reason('did not get expected column type for col "cat"')
        return 'fail'
    idx = ldefn.GetFieldIndex("str1")
    fddefn = ldefn.GetFieldDefn(idx)
    if fddefn.GetFieldTypeName(fddefn.GetType()) != 'String':
        gdaltest.post_reason('did not get expected column type for col "str1"')
        return 'fail'

    if lyr.GetGeometryColumn() != 'the_geom':
        gdaltest.post_reason('did not get expected geometry column name')
        return 'fail'

    if ldefn.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('did not get expected geometry type')
        return 'fail'

    ds = None

    try:
        os.stat('data/archsites.gfs')
        gdaltest.post_reason('did not expected .gfs -> XSD parsing failed')
        return 'fail'
    except:
        return 'success'

###############################################################################
# Test writing GML3

def ogr_gml_21(format = 'GML3'):

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Create GML3 file
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    for filename in ['tmp/gml_21.gml', 'tmp/gml_21.xsd', 'tmp/gml_21.gfs']:
        try:
            os.remove(filename)
        except:
            pass

    ds = ogr.GetDriverByName('GML').CreateDataSource('tmp/gml_21.gml', options = ['FORMAT=' + format] )
    lyr = ds.CreateLayer('firstlayer', srs = sr)
    lyr.CreateField(ogr.FieldDefn('string_field', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo')
    geom = ogr.CreateGeometryFromWkt('POINT (3 48)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    ds = None

    # Reopen the file
    ds = ogr.Open('tmp/gml_21.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('did not get expected geometry')
        return 'fail'
    ds = None

    # Test that .gml and .xsd are identical to what is expected
    f1 = open('tmp/gml_21.gml', 'rt')
    if format == 'GML3.2':
        f2 = open('data/expected_gml_gml32.gml', 'rt')
    else:
        f2 = open('data/expected_gml_21.gml', 'rt')
    line1 = f1.readline()
    line2 = f2.readline()
    while line1 != '':
        line1 = line1.strip()
        line2 = line2.strip()
        if line1 != line2:
            gdaltest.post_reason('.gml file not identical to expected')
            print(open('tmp/gml_21.gml', 'rt').read())
            return 'fail'
        line1 = f1.readline()
        line2 = f2.readline()
    f1.close()
    f2.close()

    f1 = open('tmp/gml_21.xsd', 'rt')
    if format == 'GML3':
        f2 = open('data/expected_gml_21.xsd', 'rt')
    elif format == 'GML3.2':
        f2 = open('data/expected_gml_gml32.xsd', 'rt')
    else:
        f2 = open('data/expected_gml_21_deegree3.xsd', 'rt')
    line1 = f1.readline()
    line2 = f2.readline()
    while line1 != '':
        line1 = line1.strip()
        line2 = line2.strip()
        if line1 != line2:
            gdaltest.post_reason('.xsd file not identical to expected')
            print(open('tmp/gml_21.xsd', 'rt').read())
            return 'fail'
        line1 = f1.readline()
        line2 = f2.readline()
    f1.close()
    f2.close()

    return 'success'

def ogr_gml_21_deegree3():
    return ogr_gml_21('GML3Deegree')

def ogr_gml_21_gml32():
    return ogr_gml_21('GML3.2')

###############################################################################
# Read a OpenLS DetermineRouteResponse document

def ogr_gml_22():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/paris_typical_strike_demonstration.xml')
    lyr = ds.GetLayerByName('RouteGeometry')
    if lyr is None:
        gdaltest.post_reason('cannot find RouteGeometry')
        return 'fail'
    lyr = ds.GetLayerByName('RouteInstruction')
    if lyr is None:
        gdaltest.post_reason('cannot find RouteInstruction')
        return 'fail'
    count = lyr.GetFeatureCount()
    if count != 9:
        gdaltest.post_reason('did not get expected feature count')
        print(count)
        return 'fail'

    ds = None
    return 'success'

###############################################################################
# Test that use SRS defined in global gml:Envelope if no SRS is set for any
# feature geometry

def ogr_gml_23():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.remove( 'tmp/global_geometry.gfs' )
    except:
        pass

    shutil.copy('data/global_geometry.xml', 'tmp/global_geometry.xml')

    # Here we use only the .xml file
    ds = ogr.Open('tmp/global_geometry.xml')

    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    got_wkt = sr.ExportToWkt()
    if got_wkt.find('GEOGCS["WGS 84"') == -1 or \
       got_wkt.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') != -1:
        gdaltest.post_reason('did not get expected SRS')
        print(got_wkt)
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POINT (2 49)':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    extent = lyr.GetExtent()
    if extent != (2.0, 3.0, 49.0, 50.0):
        gdaltest.post_reason('did not get expected layer extent')
        print(extent)
        return 'fail'

    return 'success'

###############################################################################
# Test that use SRS defined in global gml:Envelope if no SRS is set for any
# feature geometry

def ogr_gml_24():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.remove( 'data/global_geometry.gfs' )
    except:
        pass

    # Here we use only the .xml file and the .xsd file
    ds = ogr.Open('data/global_geometry.xml')

    lyr = ds.GetLayer(0)

    # Because we read the .xsd, we (currently) don't find the SRS
    
    #sr = lyr.GetSpatialRef()
    #got_wkt = sr.ExportToWkt()
    #if got_wkt.find('GEOGCS["WGS 84"') == -1 or \
    #   got_wkt.find('AXIS["Latitude",NORTH],AXIS["Longitude",EAST]') != -1:
    #    gdaltest.post_reason('did not get expected SRS')
    #    print(got_wkt)
    #    return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POINT (2 49)':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    extent = lyr.GetExtent()
    if extent != (2.0, 3.0, 49.0, 50.0):
        gdaltest.post_reason('did not get expected layer extent')
        print(extent)
        return 'fail'

    return 'success'

###############################################################################
# Test fixes for #3934 and #3935

def ogr_gml_25():

    if not gdaltest.have_gml_reader:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

    try:
        os.remove( 'data/curveProperty.gfs' )
    except:
        pass

    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', 'YES')
    ds = ogr.Open('data/curveProperty.xml')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)

    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    got_wkt = geom.ExportToWkt()
    if got_wkt != 'POLYGON ((14 21,6 21,6 9,14 9,22 9,22 21,14 21))':
        gdaltest.post_reason('did not get expected geometry')
        print(got_wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test writing and reading 3D geoms (GML2)

def ogr_gml_26():

    if not gdaltest.have_gml_reader:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_26.gml data/poly.shp -zfield eas_id')

    f = open('tmp/ogr_gml_26.gml', 'rt')
    content = f.read()
    f.close()
    if content.find("<gml:coord><gml:X>478315.53125</gml:X><gml:Y>4762880.5</gml:Y><gml:Z>158</gml:Z></gml:coord>") == -1:
        return 'fail'

    ds = ogr.Open('tmp/ogr_gml_26.gml')

    lyr = ds.GetLayer(0)

    if lyr.GetGeomType() != ogr.wkbPolygon25D:
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test writing and reading 3D geoms (GML3)

def ogr_gml_27():

    if not gdaltest.have_gml_reader:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_27.gml data/poly.shp -zfield eas_id -dsco FORMAT=GML3')

    f = open('tmp/ogr_gml_27.gml', 'rt')
    content = f.read()
    f.close()
    if content.find("<gml:lowerCorner>478315.53125 4762880.5 158</gml:lowerCorner>") == -1:
        return 'fail'

    ds = ogr.Open('tmp/ogr_gml_27.gml')

    lyr = ds.GetLayer(0)

    if lyr.GetGeomType() != ogr.wkbPolygon25D:
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test writing and reading layers of type wkbNone (#4154)

def ogr_gml_28():

    if not gdaltest.have_gml_reader:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_28.gml data/idlink.dbf')

    # Try with .xsd
    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    os.unlink('tmp/ogr_gml_28.xsd')

    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Try with .gfs
    ds = ogr.Open('tmp/ogr_gml_28.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test reading FME GMLs

def ogr_gml_29():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/testfmegml.gml')

    expected_results = [ [ ogr.wkbMultiPoint, 'MULTIPOINT (2 49)' ],
                         [ ogr.wkbMultiPolygon, 'MULTIPOLYGON (((2 49,3 49,3 50,2 50,2 49)))'],
                         [ ogr.wkbMultiLineString, 'MULTILINESTRING ((2 49,3 50))'],
                       ]

    for j in range(len(expected_results)):
        lyr = ds.GetLayer(j)
        if lyr.GetGeomType() != expected_results[j][0]:
            gdaltest.post_reason('layer %d, did not get expected layer geometry type' % j)
            return 'fail'
        for i in range(2):
            feat = lyr.GetNextFeature()
            geom = feat.GetGeometryRef()
            got_wkt = geom.ExportToWkt()
            if got_wkt != expected_results[j][1]:
                gdaltest.post_reason('layer %d, did not get expected geometry' % j)
                print(got_wkt)
                return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a big field and a big geometry

def ogr_gml_30():

    if not gdaltest.have_gml_reader:
        return 'skip'

    field1 = " "
    for i in range(11):
        field1 = field1 + field1

    geom = "0 1 "
    for i in range(9):
        geom = geom + geom

    data = """<FeatureCollection xmlns:gml="http://www.opengis.net/gml">
  <gml:featureMember>
    <layer1>
      <geometry><gml:LineString><gml:posList>%s</gml:posList></gml:LineString></geometry>
      <field1>A%sZ</field1>
    </layer1>
  </gml:featureMember>
</FeatureCollection>""" % (geom, field1)

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_30.gml", "wb")
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open("/vsimem/ogr_gml_30.gml")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    field1 = feat.GetField(0)
    geom_wkt = feat.GetGeometryRef().ExportToWkt()
    ds = None

    gdal.Unlink("/vsimem/ogr_gml_30.gml")
    gdal.Unlink("/vsimem/ogr_gml_30.gfs")

    if len(field1) != 2050:
        gdaltest.post_reason('did not get expected len(field1)')
        print(field1)
        print(len(field1))
        return 'fail'

    if len(geom_wkt) != 2060:
        gdaltest.post_reason('did not get expected len(geom_wkt)')
        print(geom_wkt)
        print(len(geom_wkt))
        return 'fail'

    return 'success'

###############################################################################
# Test SEQUENTIAL_LAYERS

def ogr_gml_31():

    if not gdaltest.have_gml_reader:
        return 'skip'

    gdal.SetConfigOption('GML_READ_MODE', 'SEQUENTIAL_LAYERS')
    ret = ogr_gml_29()
    gdal.SetConfigOption('GML_READ_MODE', None)

    if ret != 'success':
        return ret

    # Test reading second layer and then first layer
    gdal.SetConfigOption('GML_READ_MODE', 'SEQUENTIAL_LAYERS')
    ds = ogr.Open('data/testfmegml.gml')
    gdal.SetConfigOption('GML_READ_MODE', None)

    lyr = ds.GetLayer(1)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        gdaltest.post_reason('did not get feature when reading directly second layer')
        return 'fail'

    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        gdaltest.post_reason('did not get feature when reading back first layer')
        return 'fail'

    return 'success'

###############################################################################
# Test SEQUENTIAL_LAYERS without a .gfs

def ogr_gml_32():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Test without .xsd or .gfs
    f = gdal.VSIFOpenL("data/testfmegml.gml", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_31.gml", "wb")
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/ogr_gml_31.gml')

    lyr = ds.GetLayer(1)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        gdaltest.post_reason('did not get feature when reading directly second layer')
        return 'fail'

    ds = None

    f = gdal.VSIFOpenL("/vsimem/ogr_gml_31.gfs", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    data = str(data)

    if data.find("<SequentialLayers>true</SequentialLayers>") == -1:
        gdaltest.post_reason('did not find <SequentialLayers>true</SequentialLayers> in .gfs')
        return 'fail'

    gdal.Unlink("/vsimem/ogr_gml_31.gml")
    gdal.Unlink("/vsimem/ogr_gml_31.gfs")


    return 'success'

###############################################################################
# Test INTERLEAVED_LAYERS

def ogr_gml_33():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Test reading second layer and then first layer
    gdal.SetConfigOption('GML_READ_MODE', 'INTERLEAVED_LAYERS')
    ds = ogr.Open('data/testfmegml_interleaved.gml')
    gdal.SetConfigOption('GML_READ_MODE', None)

    read_sequence = [ [0,1],
                      [0,None],
                      [1,3],
                      [2,5],
                      [2,None],
                      [0,2],
                      [1,4],
                      [1,None],
                      [2,6],
                      [2,None],
                      [0,None],
                      [1,None],
                      [2,None] ]

    for i in range(len(read_sequence)):
        lyr = ds.GetLayer(read_sequence[i][0])
        feat = lyr.GetNextFeature()
        if feat is None:
            fid = None
        else:
            fid = feat.GetFID()
        expected_fid = read_sequence[i][1]
        if fid != expected_fid:
            gdaltest.post_reason('failed at step %d' % i)
            return 'fail'

    return 'success'

###############################################################################
# Test writing non-ASCII UTF-8 content (#4117, #4299)

def ogr_gml_34():

    if not gdaltest.have_gml_reader:
        return 'skip'

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource( '/vsimem/ogr_gml_34.gml' )
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0,  '\xc4\x80liamanu<&')
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open( '/vsimem/ogr_gml_34.gml' )
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('name') != '\xc4\x80liamanu<&':
        print(feat.GetFieldAsString('name'))
        return 'fail'
    ds = None

    gdal.Unlink( '/vsimem/ogr_gml_34.gml' )
    gdal.Unlink( '/vsimem/ogr_gml_34.gfs' )

    return 'success'

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=HUGE (#4380)

def ogr_gml_35():

    if not gdaltest.have_gml_reader:
        return 'skip'

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    if not ogrtest.have_geos():
        return 'skip'

    try:
        os.remove( 'tmp/GmlTopo-sample.sqlite' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.resolved.gml' )
    except:
        pass

    shutil.copy('data/GmlTopo-sample.xml', 'tmp/GmlTopo-sample.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'HUGE')
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)

    try:
        os.stat('tmp/GmlTopo-sample.sqlite')
        gdaltest.post_reason('did not expect tmp/GmlTopo-sample.sqlite')
        return 'fail'
    except:
        pass

    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('did not expect error')
        return 'fail'
    if ds.GetLayerCount() != 3:
        # We have an extra layer : ResolvedNodes
        gdaltest.post_reason('expected 3 layers, got %d' % ds.GetLayerCount())
        return 'fail'

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOLYGON (((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6)))'
    if ogrtest.check_feature_geometry( feat, wkt):
        print(feat.GetGeometryRef())
        return 'fail'

    ds = None

    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, wkt):
        print(feat.GetGeometryRef())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=NONE (and new GMLTopoSurface interpretation)

def ogr_gml_36(GML_FACE_HOLE_NEGATIVE = 'NO'):

    if not gdaltest.have_gml_reader:
        return 'skip'

    if GML_FACE_HOLE_NEGATIVE == 'NO':
        if not ogrtest.have_geos():
            return 'skip'

    try:
        os.remove( 'tmp/GmlTopo-sample.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.resolved.gml' )
    except:
        pass

    shutil.copy('data/GmlTopo-sample.xml', 'tmp/GmlTopo-sample.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', 'NONE')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', GML_FACE_HOLE_NEGATIVE)
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('did not expect error')
        return 'fail'

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    if GML_FACE_HOLE_NEGATIVE == 'NO':
        wkt = 'MULTIPOLYGON (((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6)))'
    else:
        wkt = 'POLYGON ((-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6),(0.2 0.2,0.2 0.4,0.4 0.4,0.5 0.2,0.5 0.1,0.5 0.0,0.2 0.0,0.2 0.2),(0.6 0.1,0.8 0.1,0.8 -0.1,0.6 -0.1,0.6 0.1))'
    if ogrtest.check_feature_geometry( feat, wkt):
        print(feat.GetGeometryRef())
        return 'fail'

    ds = None

    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', GML_FACE_HOLE_NEGATIVE)
    ds = ogr.Open('tmp/GmlTopo-sample.xml')
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)
    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, wkt):
        print(feat.GetGeometryRef())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test GML_SKIP_RESOLVE_ELEMS=NONE with old GMLTopoSurface interpretation

def ogr_gml_37():
    return ogr_gml_36('YES')

###############################################################################
# Test new GMLTopoSurface interpretation (#3934) with HUGE xlink resolver

def ogr_gml_38(resolver = 'HUGE'):

    if not gdaltest.have_gml_reader:
        return 'skip'

    if resolver == 'HUGE':
        if ogr.GetDriverByName('SQLite') is None:
            return 'skip'

    if not ogrtest.have_geos():
        return 'skip'

    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.sqlite' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.resolved.gml' )
    except:
        pass

    shutil.copy('data/sample_gml_face_hole_negative_no.xml', 'tmp/sample_gml_face_hole_negative_no.xml')

    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', resolver)
    ds = ogr.Open('tmp/sample_gml_face_hole_negative_no.xml')
    gdal.SetConfigOption('GML_SKIP_RESOLVE_ELEMS', None)
    gdal.SetConfigOption('GML_FACE_HOLE_NEGATIVE', None)

    if resolver == 'HUGE':
        try:
            os.stat('tmp/sample_gml_face_hole_negative_no.sqlite')
            gdaltest.post_reason('did not expect tmp/sample_gml_face_hole_negative_no.sqlite')
            return 'fail'
        except:
            pass

    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('did not expect error')
        return 'fail'

    lyr = ds.GetLayerByName('Suolo')
    feat = lyr.GetNextFeature()
    wkt = 'MULTIPOLYGON (((0.9 0.6,0.9 0.4,0.7 0.3,0.7 0.2,0.9 0.1,0.9 -0.1,0.6 -0.2,0.3 -0.2,0.2 -0.2,-0.1 0.0,-0.1 0.1,-0.1 0.2,0.1 0.3,0.1 0.4,-0.0 0.4,-0.1 0.5,-0.1 0.6,-0.0 0.7,0.2 0.7,0.3 0.6,0.5 0.6,0.5 0.8,0.7 0.8,0.8 0.6,0.9 0.6),(0.6 0.1,0.6 -0.1,0.8 -0.1,0.8 0.1,0.6 0.1),(0.2 0.4,0.2 0.2,0.2 0.0,0.5 0.0,0.5 0.1,0.5 0.2,0.4 0.4,0.2 0.4)))'
    if ogrtest.check_feature_geometry( feat, wkt):
        print(feat.GetGeometryRef())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test new GMLTopoSurface interpretation (#3934) with standard xlink resolver

def ogr_gml_39():
    return ogr_gml_38('NONE')

###############################################################################
# Test parsing XSD where simpleTypes not inlined, but defined elsewhere in the .xsd (#4328)

def ogr_gml_40():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/testLookForSimpleType.xml')
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('CITYNAME'))
    if fld_defn.GetWidth() != 26:
        return 'fail'

    return 'success'

###############################################################################
# Test validating against .xsd

def ogr_gml_41():

    gdaltest.have_gml_validation = False

    if not gdaltest.have_gml_reader:
        return 'skip'

    if not gdaltest.download_file('http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip', 'SCHEMAS_OPENGIS_NET.zip' ):
        return 'skip'

    ds = ogr.Open('data/expected_gml_21.gml')

    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', '/vsizip/./tmp/cache/SCHEMAS_OPENGIS_NET.zip')
    lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsInteger(0)
    feat = None

    ds.ReleaseResultSet(lyr)

    if val == 0:
        if gdal.GetLastErrorMsg().find('not implemented due to missing libxml2 support') == -1:
            return 'fail'
        return 'skip'

    gdaltest.have_gml_validation = True

    return 'success'

###############################################################################
# Test validating against .xsd

def ogr_gml_42():

    if not gdaltest.have_gml_validation:
        return 'skip'

    try:
        os.mkdir('tmp/cache/SCHEMAS_OPENGIS_NET')
    except:
        pass
    
    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET/gml')
    except:
        gdaltest.unzip( 'tmp/cache/SCHEMAS_OPENGIS_NET', 'tmp/cache/SCHEMAS_OPENGIS_NET.zip')

    ds = ogr.Open('data/expected_gml_gml32.gml')

    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', './tmp/cache/SCHEMAS_OPENGIS_NET')
    lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsInteger(0)
    feat = None

    ds.ReleaseResultSet(lyr)

    if val == 0:
        return 'fail'

    return 'success'

###############################################################################
# Test automated downloading of WFS schema

def ogr_gml_43():
    
    # The service times out
    return 'skip'

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/wfs_typefeature.gml')
    if ds is None:
        return 'fail'
    ds = None

    try:
        os.stat('data/wfs_typefeature.gfs')
        gfs_found = True
    except:
        gfs_found = False
        pass

    if gfs_found:
        if gdaltest.gdalurlopen('http://testing.deegree.org:80/deegree-wfs/services?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=app:Springs&NAMESPACE=xmlns(app=http://www.deegree.org/app)') is None:
            can_download_schema = False
        else:
            can_download_schema = gdal.GetDriverByName('HTTP') is not None 

        if can_download_schema:
            gdaltest.post_reason('.gfs found, but schema could be downloaded')
            return 'fail'

    return 'success'

###############################################################################
# Test providing a custom XSD filename

def ogr_gml_44():

    if not gdaltest.have_gml_reader:
        return 'skip'

    xsd_content = """<?xml version="1.0" encoding="UTF-8"?>
<xs:schema targetNamespace="http://ogr.maptools.org/" xmlns:ogr="http://ogr.maptools.org/" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gml="http://www.opengis.net/gml" elementFormDefault="qualified" version="1.0">
<xs:import namespace="http://www.opengis.net/gml" schemaLocation="http://schemas.opengeospatial.net/gml/2.1.2/feature.xsd"/><xs:element name="FeatureCollection" type="ogr:FeatureCollectionType" substitutionGroup="gml:_FeatureCollection"/>
<xs:complexType name="FeatureCollectionType">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureCollectionType">
      <xs:attribute name="lockId" type="xs:string" use="optional"/>
      <xs:attribute name="scope" type="xs:string" use="optional"/>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
<xs:element name="test_point" type="ogr:test_point_Type" substitutionGroup="gml:_Feature"/>
<xs:complexType name="test_point_Type">
  <xs:complexContent>
    <xs:extension base="gml:AbstractFeatureType">
      <xs:sequence>
<xs:element name="geometryProperty" type="gml:GeometryPropertyType" nillable="true" minOccurs="1" maxOccurs="1"/>
    <xs:element name="dbl" nillable="true" minOccurs="0" maxOccurs="1">
      <xs:simpleType>
        <xs:restriction base="xs:decimal">
          <xs:totalDigits value="32"/>
          <xs:fractionDigits value="3"/>
        </xs:restriction>
      </xs:simpleType>
    </xs:element>
      </xs:sequence>
    </xs:extension>
  </xs:complexContent>
</xs:complexType>
</xs:schema>"""

    gdal.FileFromMemBuffer('/vsimem/ogr_gml_44.xsd', xsd_content)

    ds = ogr.Open('data/test_point.gml,xsd=/vsimem/ogr_gml_44.xsd')
    lyr = ds.GetLayer(0)

    # fid and dbl
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_gml_44.xsd')

    return 'success'

###############################################################################
# Test PREFIX and TARGET_NAMESPACE creation options

def ogr_gml_45():

    if not gdaltest.have_gml_reader:
        return 'skip'

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('/vsimem/ogr_gml_45.gml', options = ['PREFIX=foo', 'TARGET_NAMESPACE=http://bar/'])
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('dbl', ogr.OFTReal))

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('str', 'str')
    dst_feat.SetField('int', 1)
    dst_feat.SetField('dbl', 2.34)

    lyr.CreateFeature( dst_feat )

    dst_feat = None
    ds = None

    if not gdaltest.have_gml_validation:
        gdal.Unlink('/vsimem/ogr_gml_45.gml')
        gdal.Unlink('/vsimem/ogr_gml_45.xsd')
        return 'skip'

    # Validate document

    ds = ogr.Open('/vsimem/ogr_gml_45.gml')

    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', './tmp/cache/SCHEMAS_OPENGIS_NET')
    lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
    gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsInteger(0)
    feat = None

    ds.ReleaseResultSet(lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_45.gml')
    gdal.Unlink('/vsimem/ogr_gml_45.xsd')

    if val == 0:
        return 'fail'

    return 'success'


###############################################################################
# Validate different kinds of GML files

def ogr_gml_46():

    if not gdaltest.have_gml_validation:
        return 'skip'

    wkt_list = [ '',
                 'POINT (0 1)',
                 # 'POINT (0 1 2)',
                 'LINESTRING (0 1,2 3)',
                 # 'LINESTRING (0 1 2,3 4 5)',
                 'POLYGON ((0 0,0 1,1 1,1 0,0 0))',
                 # 'POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))',
                 'MULTIPOINT (0 1)',
                 # 'MULTIPOINT (0 1 2)',
                 'MULTILINESTRING ((0 1,2 3))',
                 # 'MULTILINESTRING ((0 1 2,3 4 5))',
                 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))',
                 # 'MULTIPOLYGON (((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))',
                 'GEOMETRYCOLLECTION (POINT (0 1))',
                 # 'GEOMETRYCOLLECTION (POINT (0 1 2))'
                ]

    format_list = [ 'GML2', 'GML3', 'GML3Deegree', 'GML3.2' ]

    for wkt in wkt_list:
        for format in format_list:
            drv = ogr.GetDriverByName('GML')
            ds = drv.CreateDataSource('/vsimem/ogr_gml_46.gml', options = ['FORMAT=%s' % format])
            if wkt != '':
                geom = ogr.CreateGeometryFromWkt(wkt)
                geom_type = geom.GetGeometryType()
                srs = osr.SpatialReference()
                srs.ImportFromEPSG(4326)
            else:
                geom = None
                geom_type = ogr.wkbNone
                srs = None

            lyr = ds.CreateLayer('test', geom_type = geom_type, srs = srs)

            lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
            lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
            lyr.CreateField(ogr.FieldDefn('dbl', ogr.OFTReal))

            dst_feat = ogr.Feature( lyr.GetLayerDefn() )
            dst_feat.SetField('str', 'str')
            dst_feat.SetField('int', 1)
            dst_feat.SetField('dbl', 2.34)
            dst_feat.SetGeometry(geom)

            lyr.CreateFeature( dst_feat )

            dst_feat = None
            ds = None

            # Validate document

            ds = ogr.Open('/vsimem/ogr_gml_46.gml')

            lyr = ds.GetLayer(0)
            feat = lyr.GetNextFeature()
            got_geom = feat.GetGeometryRef()

            if got_geom is None:
                got_geom_wkt = ''
            else:
                got_geom_wkt = got_geom.ExportToWkt()

            if got_geom_wkt != wkt:
                gdaltest.post_reason('geometry do not match')
                print('got %s, expected %s' % (got_geom_wkt, wkt))

            feat = None

            gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', './tmp/cache/SCHEMAS_OPENGIS_NET')
            lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
            gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

            feat = lyr.GetNextFeature()
            val = feat.GetFieldAsInteger(0)
            feat = None

            ds.ReleaseResultSet(lyr)
            ds = None

            if val == 0:
                gdaltest.post_reason('validation failed for format=%s, wkt=%s' % (format, wkt))

                f = gdal.VSIFOpenL('/vsimem/ogr_gml_46.gml', 'rb')
                content = gdal.VSIFReadL(1, 10000, f)
                gdal.VSIFCloseL(f)
                print(content)

                f = gdal.VSIFOpenL('/vsimem/ogr_gml_46.xsd', 'rb')
                content = gdal.VSIFReadL(1, 10000, f)
                gdal.VSIFCloseL(f)
                print(content)

            gdal.Unlink('/vsimem/ogr_gml_46.gml')
            gdal.Unlink('/vsimem/ogr_gml_46.xsd')

            if val == 0:
                return 'fail'

        # Only minor schema changes
        if format == 'GML3Deegree':
            break

    return 'success'

###############################################################################
# Test validation of WFS GML documents

def ogr_gml_47():

    if not gdaltest.have_gml_validation:
        return 'skip'

    filenames = [ 'data/wfs10.xml', 'data/wfs11.xml', 'data/wfs20.xml' ]

    for filename in filenames:

        # Validate document

        ds = ogr.Open(filename)

        gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', './tmp/cache/SCHEMAS_OPENGIS_NET')
        lyr = ds.ExecuteSQL('SELECT ValidateSchema()')
        gdal.SetConfigOption('GDAL_OPENGIS_SCHEMAS', None)

        feat = lyr.GetNextFeature()
        val = feat.GetFieldAsInteger(0)
        feat = None

        ds.ReleaseResultSet(lyr)
        ds = None

        if val == 0:
            gdaltest.post_reason('validation failed for file=%s' % filename)
            return 'fail'

    return 'success'

###############################################################################
# Test that we can parse some particular .xsd files that have the geometry
# field declared as :
#    <xsd:element name="geometry" minOccurs="0" maxOccurs="1">
#    <xsd:complexType>
#        <xsd:sequence>
#        <xsd:element ref="gml:_Geometry"/>
#        </xsd:sequence>
#    </xsd:complexType>
#    </xsd:element>

def ogr_gml_48():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/schema_with_geom_in_complextype.xml')
    lyr = ds.GetLayer(0)

    if lyr.GetGeomType() != ogr.wkbUnknown:
        gdaltest.post_reason('failure')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test a pseudo Inspire GML file

def ogr_gml_49():

    if not gdaltest.have_gml_reader:
        return 'skip'

    xsd_content = """<ogr:FeatureCollection xmlns:gml="http://www.opengis.net/gml" xmlns:ogr="http://ogr.maptools.org/">
  <gml:featureMember>
    <ogr:test>
      <ogr:geometry><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>2,49 2,50 3,50 3,49 2,49</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></ogr:geometry>
      <ogr:otherGeometry><gml:Point><gml:pos>-2 -49</gml:pos></gml:Point></ogr:otherGeometry>
    </ogr:test>
  </gml:featureMember>
</ogr:FeatureCollection>
"""

    gdal.FileFromMemBuffer('/vsimem/ogr_gml_49.gml', xsd_content)

    ds = ogr.Open('/vsimem/ogr_gml_49.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().GetGeometryType() != ogr.wkbPolygon:
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_gml_49.gml')
    gdal.Unlink('/vsimem/ogr_gml_49.gfs')

    return 'success'

###############################################################################
# Test support for StringList, IntegerList, RealList

def ogr_gml_50():

    if not gdaltest.have_gml_reader:
        return 'skip'

    drv = ogr.GetDriverByName('GML')
    ds = drv.CreateDataSource('/vsimem/ogr_gml_50.gml')
    lyr = ds.CreateLayer( 'listlayer' )
    field_defn = ogr.FieldDefn( 'stringlist', ogr.OFTStringList )
    lyr.CreateField( field_defn )
    field_defn = ogr.FieldDefn( 'intlist', ogr.OFTIntegerList )
    lyr.CreateField( field_defn )
    field_defn = ogr.FieldDefn( 'reallist', ogr.OFTRealList )
    lyr.CreateField( field_defn )
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    feat.SetFieldStringList(0, ['a', 'b'])
    feat.SetFieldIntegerList(1, [2, 3])
    feat.SetFieldDoubleList(2, [4.56, 5.67])
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gml_50.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsStringList(lyr.GetLayerDefn().GetFieldIndex('stringlist')) != ['a', 'b']:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    if feat.GetFieldAsIntegerList(lyr.GetLayerDefn().GetFieldIndex('intlist')) != [2, 3]:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    if feat.GetFieldAsDoubleList(lyr.GetLayerDefn().GetFieldIndex('reallist')) != [4.56, 5.67]:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gml_50.gml')
    gdal.Unlink('/vsimem/ogr_gml_50.xsd')

    return 'success'

###############################################################################
# Test -dsco WRITE_FEATURE_BOUNDED_BY=no -dsco STRIP_PREFIX=YES

def ogr_gml_51():

    if not gdaltest.have_gml_reader:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    for format in ['GML2', 'GML3']:

        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f GML tmp/ogr_gml_51.gml data/poly.shp -dsco FORMAT=%s -dsco WRITE_FEATURE_BOUNDED_BY=no -dsco STRIP_PREFIX=YES' % format)

        f = open('tmp/ogr_gml_51.gml', 'rt')
        content = f.read()
        f.close()
        if content.find("<FeatureCollection") == -1:
            gdaltest.post_reason('fail')
            print(content)
            return 'fail'
        if format == 'GML3':
            if content.find("<featureMember>") == -1:
                gdaltest.post_reason('fail')
                print(content)
                return 'fail'
        if content.find("""<poly""") == -1:
            gdaltest.post_reason('fail')
            print(content)
            return 'fail'
        if content.find("""<AREA>215229.266</AREA>""") == -1:
            gdaltest.post_reason('fail')
            print(content)
            return 'fail'

        if content.find("""<gml:boundedBy><gml:Envelope><gml:lowerCorner>479647""") != -1:
            gdaltest.post_reason('fail')
            print(content)
            return 'fail'

        ds = ogr.Open('tmp/ogr_gml_51.gml')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        if feat is None:
            return 'fail'
        ds = None

    return 'success'

###############################################################################
# Test reading MTKGML files

def ogr_gml_52():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.remove( 'data/fake_mtkgml.gfs' )
    except:
        pass

    for i in range(2):

        ds = ogr.Open('data/fake_mtkgml.xml')

        lyr = ds.GetLayerByName('A')
        if lyr.GetGeomType() != ogr.wkbPoint25D:
            gdaltest.post_reason('fail')
            return 'fail'
        srs = lyr.GetSpatialRef()
        if srs is None:
            gdaltest.post_reason('fail')
            return 'fail'
        wkt = srs.ExportToWkt()
        if wkt.find('3067') < 0:
            gdaltest.post_reason('fail')
            print(wkt)
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat.GetField('gid') != '1' or \
        feat.GetField('regular_attribute') != 5 or \
        feat.GetField('foo_href') != 'some_ref' or \
        feat.GetField('teksti') != 'En francais !' or \
        feat.GetField('teksti_kieli') != 'fr' or \
        ogrtest.check_feature_geometry( feat, 'POINT (280000 7000000 0)') != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        lyr = ds.GetLayerByName('B')
        if lyr.GetGeomType() != ogr.wkbPolygon25D:
            gdaltest.post_reason('fail')
            return 'fail'
        srs = lyr.GetSpatialRef()
        if srs is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat, 'POLYGON ((280000 7000000 0,281000 7000000 0,281000 7001000 0,280000 7001000 0,280000 7000000 0))') != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        lyr = ds.GetLayerByName('C')
        if lyr.GetGeomType() != ogr.wkbLineString25D:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat, 'LINESTRING (280000 7000000 0,281000 7000000 0,281000 7001000 0,280000 7001000 0,280000 7000000 0)') != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        ds = None

    os.remove( 'data/fake_mtkgml.gfs' )

    return 'success'

###############################################################################
# Test that we don't recognize .xsd files themselves

def ogr_gml_53():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/archsites.xsd')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds =  None

    return 'success'

###############################################################################
# Test that we can open an empty GML datasource (#249, #5205)

def ogr_gml_54():

    if not gdaltest.have_gml_reader:
        return 'skip'

    try:
        os.unlink('data/empty.gfs')
    except:
        pass

    ds = ogr.Open('data/empty.gml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds =  None

    # with .gfs now
    ds = ogr.Open('data/empty.gml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds =  None

    try:
        os.unlink('data/empty.gfs')
    except:
        pass

    return 'success'

###############################################################################
# Test support for <xs:include> in schemas
# Necessary for Finnish NLS data

def ogr_gml_55():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/ogr_gml_55.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        return 'fail'
    ds = None

    try:
        os.unlink('data/ogr_gml_55.gfs')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test support for gml:FeaturePropertyType and multiple geometry field
# Necessary for Finnish NLS data

def ogr_gml_56():

    if not gdaltest.have_gml_reader:
        return 'skip'

    gdal.SetConfigOption('GML_REGISTRY', 'data/ogr_gml_56_registry.xml')
    ds = ogr.Open('data/ogr_gml_56.gml')
    gdal.SetConfigOption('GML_REGISTRY', None)
    lyr = ds.GetLayerByName('mainFeature')
    if lyr.GetSpatialRef() is None:
        gdaltest.post_reason('fail')
        return 'failure'
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(feat.GetFieldIndex('subFeatureProperty_href')) != '#subFeature.0':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetFieldAsStringList(feat.GetFieldIndex('subFeatureRepeatedProperty_href')) != ['#subFeatureRepeated.0','#subFeatureRepeated.1']:
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (10 10)':
        gdaltest.post_reason('fail')
        return 'failure'

    lyr = ds.GetLayerByName('subFeature')
    if lyr.GetLayerDefn().GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'failure'
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsStringList(feat.GetFieldIndex('subFeatureRepeatedProperty_href')) != ['#subFeatureRepeated.2']:
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetField('foo') != 'bar':
        gdaltest.post_reason('fail')
        return 'failure'

    lyr = ds.GetLayerByName('subFeatureRepeated')
    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'subFeatureRepeated.2':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetField('bar') != 'baz':
        gdaltest.post_reason('fail')
        return 'failure'
    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'subFeatureRepeated.0':
        gdaltest.post_reason('fail')
        return 'failure'
    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'subFeatureRepeated.1':
        gdaltest.post_reason('fail')
        return 'failure'
    ds = None

    try:
        os.unlink('data/ogr_gml_56.gfs')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test write support for multiple geometry field

def ogr_gml_57():

    if not gdaltest.have_gml_reader:
        return 'skip'

    for i in range(4):
        options = []
        if i == 3:
            options = [ 'FORMAT=GML3.2' ]
        ds = ogr.GetDriverByName('GML').CreateDataSource('/vsimem/ogr_gml_57.gml', options = options)
        if ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        lyr = ds.CreateLayer('myLayer', geom_type = ogr.wkbNone)
        if lyr.TestCapability(ogr.OLCCreateGeomField) != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        geomfielddefn = ogr.GeomFieldDefn('first_geometry', ogr.wkbPoint)
        if i == 1 or i == 2:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32630)
            geomfielddefn.SetSpatialRef(sr)
        lyr.CreateGeomField(geomfielddefn)
        geomfielddefn = ogr.GeomFieldDefn('second_geometry', ogr.wkbLineString)
        if i == 1:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32630)
            geomfielddefn.SetSpatialRef(sr)
        elif i == 2:
            sr = osr.SpatialReference()
            sr.ImportFromEPSG(32631)
            geomfielddefn.SetSpatialRef(sr)
        lyr.CreateGeomField(geomfielddefn)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt('POINT (0 1)'))
        feat.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt('LINESTRING (2 3,4 5)'))
        lyr.CreateFeature(feat)
        feat = None
        ds = None

        if False:
            f = gdal.VSIFOpenL('/vsimem/ogr_gml_57.gml', 'rb')
            print(gdal.VSIFReadL(1, 1000, f))
            gdal.VSIFCloseL(f)

        ds = ogr.Open('/vsimem/ogr_gml_57.gml')
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        if i == 1 and feat.GetGeomFieldRef(0).GetSpatialReference().ExportToWkt().find('32630') < 0:
            gdaltest.post_reason('fail')
            return 'failure'
        if i == 1 and feat.GetGeomFieldRef(1).GetSpatialReference().ExportToWkt().find('32630') < 0:
            gdaltest.post_reason('fail')
            return 'failure'
        if i == 2 and feat.GetGeomFieldRef(1).GetSpatialReference().ExportToWkt().find('32631') < 0:
            gdaltest.post_reason('fail')
            return 'failure'
        if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (0 1)':
            gdaltest.post_reason('fail')
            return 'failure'
        if feat.GetGeomFieldRef(1).ExportToWkt() != 'LINESTRING (2 3,4 5)':
            gdaltest.post_reason('fail')
            return 'failure'
        ds = None

        gdal.Unlink('/vsimem/ogr_gml_57.gml')
        gdal.Unlink('/vsimem/ogr_gml_57.xsd')

    return 'success'

###############################################################################
# Test support for Inspire Cadastral schemas

def ogr_gml_58():

    if not gdaltest.have_gml_reader:
        return 'skip'

    ds = ogr.Open('data/inspire_cadastralparcel.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetName() != 'geometry':
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetType() != ogr.wkbMultiPolygon:
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(1).GetName() != 'referencePoint':
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(1).GetType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralParcel-01'),
                 ('areaValue', 10.0),
                 ('areaValue_uom', 'm2'),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                 ('inspireId_localId', 'CadastralParcel-01-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('label', 'label'),
                 ('nationalCadastralReference', 'nationalCadastralReference'),
                 ('validFrom', '2002-01-01T00:00:00.0Z'),
                 ('validTo', '2003-01-01T00:00:00.0Z'),
                 ('basicPropertyUnit_href', ['#BPU.1','#BPU.2'] ),
                 ('administrativeUnit_href', '#AU.1'),
                 ('zoning_href', '#CZ.1') ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (2.5 49.5)':
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralParcel-02'),
                 ('areaValue', None),
                 ('areaValue_uom', None),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', None),
                 ('inspireId_localId', 'CadastralParcel-02-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('label', 'label'),
                 ('nationalCadastralReference', 'nationalCadastralReference'),
                 ('validFrom', None),
                 ('validTo', None),
                 ('basicPropertyUnit_href', None ),
                 ('administrativeUnit_href', None),
                 ('zoning_href', None) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(1) is not None:
        gdaltest.post_reason('fail')
        return 'failure'
    feat = None
    lyr = None
    ds = None


    ds = ogr.Open('data/inspire_basicpropertyunit.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'BasicPropertyUnit-01'),
                 ('inspireId_localId', 'BasicPropertyUnit-01-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('nationalCadastralReference', 'nationalCadastralReference'),
                 ('areaValue', 10.0),
                 ('areaValue_uom', 'm2'),
                 ('validFrom', '2000-01-01T00:00:00.0Z'),
                 ('validTo', '2001-01-01T00:00:00.0Z'),
                 ('beginLifespanVersion', '2002-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', '2003-01-01T00:00:00.0Z'),
                 ('administrativeUnit_href', '#AU.1') ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'BasicPropertyUnit-02'),
                 ('inspireId_localId', 'BasicPropertyUnit-02-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('nationalCadastralReference', 'nationalCadastralReference'),
                 ('areaValue', None),
                 ('areaValue_uom', None),
                 ('validFrom', '2000-01-01T00:00:00.0Z'),
                 ('validTo', None),
                 ('beginLifespanVersion', '2002-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', None),
                 ('administrativeUnit_href', None) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('data/inspire_cadastralboundary.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetGeomFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetName() != 'geometry':
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralBoundary-01'),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                 ('estimatedAccuracy', 1.0),
                 ('estimatedAccuracy_uom', 'm'),
                 ('inspireId_localId', 'CadastralBoundary-01-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('validFrom', '2002-01-01T00:00:00.0Z'),
                 ('validTo', '2003-01-01T00:00:00.0Z'),
                 ('parcel_href', ['#Parcel.1','#Parcel.2'] ) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'LINESTRING (2 49,3 50)':
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralBoundary-02'),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', None),
                 ('estimatedAccuracy', None),
                 ('estimatedAccuracy_uom', None),
                 ('inspireId_localId', 'CadastralBoundary-02-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('validFrom', None),
                 ('validTo', None),
                 ('parcel_href', None ) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'LINESTRING (2 49,3 50)':
        gdaltest.post_reason('fail')
        return 'failure'
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('data/inspire_cadastralzoning.xml')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetName() != 'geometry':
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(0).GetType() != ogr.wkbMultiPolygon:
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(1).GetName() != 'referencePoint':
        gdaltest.post_reason('fail')
        return 'failure'
    if lyr_defn.GetGeomFieldDefn(1).GetType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralZoning-01'),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', '2001-01-01T00:00:00.0Z'),
                 ('estimatedAccuracy', 1.0),
                 ('estimatedAccuracy_uom', 'm'),
                 ('inspireId_localId', 'CadastralZoning-01-localId'),
                 ('inspireId_namespace', 'namespace'),
                 ('label', 'label'),
                 ('level', '3'),
                 ('levelName', ['English', 'Francais', 'Deutsch']),
                 ('levelName_locale', ['en', 'fr', 'de']),
                 ('name_language', ['language']),
                 ('name_nativeness', ['nativeness']),
                 ('name_nameStatus', ['nameStatus']),
                 ('name_pronunciation', None),
                 ('name_spelling_text', ['text']),
                 ('name_spelling_script', ['script']),
                 ('nationalCadastalZoningReference', 'nationalCadastalZoningReference'),
                 ('validFrom', '2002-01-01T00:00:00.0Z'),
                 ('validTo', '2003-01-01T00:00:00.0Z'),
                 ('upperLevelUnit_href', '#ulu.1') ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'POINT (2.5 49.5)':
        gdaltest.post_reason('fail')
        return 'failure'

    feat = lyr.GetNextFeature()
    expected = [ ('gml_id', 'CadastralZoning-02'),
                 ('beginLifespanVersion', '2000-01-01T00:00:00.0Z'),
                 ('endLifespanVersion', None),
                 ('estimatedAccuracy', None),
                 ('estimatedAccuracy_uom', None),
                 ('inspireId_localId', None),
                 ('inspireId_namespace', None),
                 ('label', 'label'),
                 ('level', '3'),
                 ('levelName', ['English']),
                 ('levelName_locale', ['en']),
                 ('name_language', None),
                 ('name_nativeness', None),
                 ('name_nameStatus', None),
                 ('name_pronunciation', None),
                 ('name_spelling_text', None),
                 ('name_spelling_script', None),
                 ('nationalCadastalZoningReference', 'nationalCadastalZoningReference'),
                 ('validFrom', None),
                 ('validTo', None),
                 ('upperLevelUnit_href', None) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'MULTIPOLYGON (((2 49,2 50,3 50,3 49)))':
        gdaltest.post_reason('fail')
        return 'failure'
    if feat.GetGeomFieldRef(1) is not None:
        gdaltest.post_reason('fail')
        return 'failure'
    feat = None
    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test GFS conditions

def ogr_gml_59():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/testcondition.gml').st_mtime
        gfs_mtime = os.stat('data/testcondition.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/testcondition.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/testcondition.gml')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    expected = [ ('name_en', 'English name'),
                 ('name_fr', 'Nom francais'),
                 ('name_others_lang', ['de']),
                 ('name_others', ['Deutsche name']) ]
    for (key,val) in expected:
        if feat.GetField(key) != val:
            print(key)
            print(val)
            print(feat.GetField(key))
            gdaltest.post_reason('fail')
            return 'failure'
    feat = None
    lyr = None
    ds = None

    return 'success'

########################################################
# Test reading WFS 2.0 GetFeature documents with wfs:FeatureCollection
# as a wfs:member of the top wfs:FeatureCollection

def ogr_gml_60():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Make sure the .gfs file is more recent that the .gml one
    try:
        os.unlink('data/wfs_200_multiplelayers.gfs')
    except:
        pass

    for i in range(2):
        ds = ogr.Open('data/wfs_200_multiplelayers.gml')
        lyr = ds.GetLayerByName('road')
        if lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'failure'
        feat = lyr.GetNextFeature()
        if feat.GetField('gml_id') != 'road.21':
            gdaltest.post_reason('fail')
            return 'failure'
        lyr = ds.GetLayerByName('popplace')
        if lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'failure'
        feat = lyr.GetNextFeature()
        if feat.GetField('gml_id') != 'popplace.BACMK':
            gdaltest.post_reason('fail')
            return 'failure'
        ds = None

    try:
        os.unlink('data/wfs_200_multiplelayers.gfs')
    except:
        pass

    return 'success'

###############################################################################
# Test reading a element specified with a full path in <ElementPath>

def ogr_gml_61():

    if not gdaltest.have_gml_reader:
        return 'skip'

    # Make sure the .gfs file is more recent that the .gml one
    try:
        gml_mtime = os.stat('data/gmlsubfeature.gml').st_mtime
        gfs_mtime = os.stat('data/gmlsubfeature.gfs').st_mtime
        touch_gfs = gfs_mtime <= gml_mtime
    except:
        touch_gfs = True
    if touch_gfs:
        print('Touching .gfs file')
        f = open('data/gmlsubfeature.gfs', 'rb+')
        data = f.read(1)
        f.seek(0, 0)
        f.write(data)
        f.close()

    ds = ogr.Open('data/gmlsubfeature.gml')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('did not get expected geometry column name')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'Object.1' or feat.GetField('foo') != 'bar':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON ((2 48,2 49,3 49,3 48,2 48))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'Object.2' or feat.GetField('foo') != 'baz':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON ((2 -48,2 -49,3 -49,3 -48,2 -48))':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    ds = None
    return 'success'

###############################################################################
#  Cleanup

def ogr_gml_cleanup():
    if not gdaltest.have_gml_reader:
        return 'skip'

    gdal.SetConfigOption( 'GML_SKIP_RESOLVE_ELEMS', None )
    gdal.SetConfigOption( 'GML_SAVE_RESOLVED_TO', None )
    
    gdaltest.clean_tmp()
    
    return ogr_gml_clean_files()


def ogr_gml_clean_files():
    try:
        os.remove( 'data/bom.gfs' )
    except:
        pass
    try:
        os.remove( 'data/utf8.gfs' )
    except:
        pass
    try:
        os.remove( 'data/ticket_2349_test_1.gfs' )
    except:
        pass
    try:
        os.remove( 'data/citygml.gfs' )
    except:
        pass
    try:
        os.remove( 'data/gnis_pop_100.gfs' )
    except:
        pass
    try:
        os.remove( 'data/gnis_pop_110.gfs' )
    except:
        pass
    try:
        os.remove( 'data/paris_typical_strike_demonstration.gfs' )
    except:
        pass
    try:
        os.remove( 'data/global_geometry.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/global_geometry.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/global_geometry.xml' )
    except:
        pass
    try:
        os.remove( 'data/curveProperty.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/ogr_gml_26.gml' )
        os.remove( 'tmp/ogr_gml_26.xsd' )
    except:
        pass
    try:
        os.remove( 'tmp/ogr_gml_27.gml' )
        os.remove( 'tmp/ogr_gml_27.xsd' )
    except:
        pass
    try:
        os.remove( 'tmp/ogr_gml_28.gml' )
        os.remove( 'tmp/ogr_gml_28.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.sqlite' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.resolved.gml' )
    except:
        pass
    try:
        os.remove( 'tmp/GmlTopo-sample.xml' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.sqlite' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.resolved.gml' )
    except:
        pass
    try:
        os.remove( 'tmp/sample_gml_face_hole_negative_no.xml' )
    except:
        pass
    try:
        os.remove( 'data/wfs_typefeature.gfs' )
    except:
        pass
    try:
        os.remove( 'tmp/ogr_gml_51.gml' )
        os.remove( 'tmp/ogr_gml_51.xsd' )
    except:
        pass
    files = os.listdir('data')
    for filename in files:
        if len(filename) > 13 and filename[-13:] == '.resolved.gml':
            os.unlink('data/' + filename)

    return 'success'

gdaltest_list = [ 
    ogr_gml_clean_files,
    ogr_gml_1,
    ogr_gml_2,
    ogr_gml_3,
    ogr_gml_4,
    ogr_gml_5,
    ogr_gml_6,
    ogr_gml_7,
    ogr_gml_8,
    ogr_gml_9,
    ogr_gml_10,
    ogr_gml_11,
    ogr_gml_12,
    ogr_gml_13,
    ogr_gml_14,
    ogr_gml_15,
    ogr_gml_16,
    ogr_gml_17,
    ogr_gml_18,
    ogr_gml_19,
    ogr_gml_20,
    ogr_gml_21,
    ogr_gml_21_deegree3,
    ogr_gml_21_gml32,
    ogr_gml_22,
    ogr_gml_23,
    ogr_gml_24,
    ogr_gml_25,
    ogr_gml_26,
    ogr_gml_27,
    ogr_gml_28,
    ogr_gml_29,
    ogr_gml_30,
    ogr_gml_31,
    ogr_gml_32,
    ogr_gml_33,
    ogr_gml_34,
    ogr_gml_35,
    ogr_gml_36,
    ogr_gml_37,
    ogr_gml_38,
    ogr_gml_39,
    ogr_gml_40,
    ogr_gml_41,
    ogr_gml_42,
    ogr_gml_43,
    ogr_gml_44,
    ogr_gml_45,
    ogr_gml_46,
    ogr_gml_47,
    ogr_gml_48,
    ogr_gml_49,
    ogr_gml_50,
    ogr_gml_51,
    ogr_gml_52,
    ogr_gml_53,
    ogr_gml_54,
    ogr_gml_55,
    ogr_gml_56,
    ogr_gml_57,
    ogr_gml_58,
    ogr_gml_59,
    ogr_gml_60,
    ogr_gml_61,
    ogr_gml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


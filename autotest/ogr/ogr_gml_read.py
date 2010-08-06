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

    gml_ds = ogr.Open( 'data/test_point.gml' )
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
    gdal.SetConfigOption( 'GML_SKIP_RESOLVE_ELEMS', 'ALL' )

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
#  Cleanup

def ogr_gml_cleanup():
    if not gdaltest.have_gml_reader:
        return 'skip'
    
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
    ogr_gml_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


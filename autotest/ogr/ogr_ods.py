#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ODS driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Check

def ogr_ods_check(ds, variant = False):

    if ds.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 8:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'Feuille1':
        gdaltest.post_reason('bad layer name')
        return 'fail'

    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    if lyr.GetSpatialRef() != None:
        gdaltest.post_reason('bad spatial ref')
        return 'fail'

    if lyr.GetFeatureCount() != 26:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    if lyr.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(6)
    if lyr.GetName() != 'Feuille7':
        gdaltest.post_reason('bad layer name')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldCount() != 12:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    type_array = [ ogr.OFTString,
                   ogr.OFTInteger,
                   ogr.OFTReal,
                   ogr.OFTReal,
                   variant and ogr.OFTString or ogr.OFTDate,
                   ogr.OFTDateTime,
                   ogr.OFTReal,
                   ogr.OFTTime,
                   ogr.OFTReal,
                   ogr.OFTInteger,
                   ogr.OFTReal,
                   ogr.OFTDateTime ]

    for i in range(len(type_array)):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != type_array[i]:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'val' or \
       feat.GetFieldAsInteger(1) != 23 or \
       feat.GetFieldAsDouble(2) != 3.45 or \
       feat.GetFieldAsDouble(3) != 0.52 or \
       feat.GetFieldAsString(4) != '2012/01/22' or \
       feat.GetFieldAsString(5) != '2012/01/22 18:49:00':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.IsFieldSet(2):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Basic tests

def ogr_ods_1():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    if drv.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('data/test.ods')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    return ogr_ods_check(ds)

###############################################################################
# Basic tests

def ogr_ods_kspread_1():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    if drv.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('data/test_kspread.ods')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 8:
        gdaltest.post_reason('bad layer count')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'Feuille1':
        gdaltest.post_reason('bad layer name')
        return 'fail'

    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('bad layer geometry type')
        return 'fail'

    if lyr.GetSpatialRef() != None:
        gdaltest.post_reason('bad spatial ref')
        return 'fail'

    if lyr.GetFeatureCount() != 26:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    if lyr.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(6)
    if lyr.GetName() != 'Feuille7':
        gdaltest.post_reason('bad layer name')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldCount() != 12:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    type_array = [ ogr.OFTString,
                   ogr.OFTInteger,
                   ogr.OFTReal,
                   ogr.OFTReal,
                   ogr.OFTDate,
                   ogr.OFTString, #ogr.OFTDateTime,
                   ogr.OFTReal,
                   ogr.OFTTime,
                   ogr.OFTReal,
                   ogr.OFTInteger,
                   ogr.OFTReal,
                   ogr.OFTString, #ogr.OFTDateTime
                  ]

    for i in range(len(type_array)):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != type_array[i]:
            gdaltest.post_reason('fail')
            print(i)
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'val' or \
       feat.GetFieldAsInteger(1) != 23 or \
       feat.GetFieldAsDouble(2) != 3.45 or \
       feat.GetFieldAsDouble(3) != 0.52 or \
       feat.GetFieldAsString(4) != '2012/01/22' or \
       feat.GetFieldAsString(5) != '22/01/2012 18:49:00': # 2012/01/22 18:49:00
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.IsFieldSet(2):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test OGR_ODS_HEADERS = DISABLE

def ogr_ods_2():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_ODS_HEADERS', 'DISABLE')
    ds = ogr.Open('data/test.ods')

    lyr = ds.GetLayerByName('Feuille7')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    gdal.SetConfigOption('OGR_ODS_HEADERS', None)

    return 'success'

###############################################################################
# Test OGR_ODS_FIELD_TYPES = STRING

def ogr_ods_3():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_ODS_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/test.ods')

    lyr = ds.GetLayerByName('Feuille7')

    if lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('OGR_ODS_FIELD_TYPES', None)

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_ods_4():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.ods')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test write support

def ogr_ods_5():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f ODS tmp/test.ods data/test.ods')

    ds = ogr.Open('tmp/test.ods')
    ret = ogr_ods_check(ds, variant = True)
    ds = None

    os.unlink('tmp/test.ods')

    return ret

###############################################################################
# Test formula evaluation

def ogr_ods_6():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    src_ds = ogr.Open('ODS:data/content_formulas.xml')
    out_ds = ogr.GetDriverByName('CSV').CopyDataSource(src_ds, '/vsimem/content_formulas.csv')
    del out_ds
    src_ds = None

    fp = gdal.VSIFOpenL('/vsimem/content_formulas.csv', 'rb')
    res = gdal.VSIFReadL(1,10000,fp)
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/content_formulas.csv')

    res = res.decode('ascii').split()

    expected_res = """Field1,Field2,Field3,Field4,Field5,Field6,Field7,Field8,Field9,Field10,Field11,Field12,Field13,Field14,Field15,Field16,Field17,Field18,Field19,Field20,Field21,Field22,Field23,Field24,Field25,Field26,Field27,Field28,Field29,Field30,Field31,Field32
of:=[.B1],of:=[.C1],of:=[.A1],,,,,,,,,,,,,,,,,,,,,,,,,,,,,
1,1,1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
ab,ab,ab,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
1,a,,3.5,MIN,1,MIN,3.5,SUM,4.5,AVERAGE,2.25,COUNT,2,COUNTA,3,,,,,,,,,,,,,,,,
abcdef,6,,a,abcdef,,f,abcdef,"of:=MID([.A5];0;1)",,a,abcdef,,a,ef,ef,,,,,,,,,,,,,,,,
1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
AB,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
2,2,0,3,1,0,0,1,1,1,0,0,0,1,1,0,,,,,,,,,,,,,,,,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
""".split()

    if res != expected_res:
        gdaltest.post_reason('did not get expected result')
        print(res)
        return 'fail'

    return 'success'

###############################################################################
# Test update support

def ogr_ods_7():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    try:
        os.unlink('tmp/ogr_ods_7.ods')
    except:
        pass
    shutil.copy('data/test.ods', 'tmp/ogr_ods_7.ods')

    ds = ogr.Open('tmp/ogr_ods_7.ods', update = 1)
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        gdaltest.post_reason('did not get expected FID')
        feat.DumpReadabe()
        return 'fail'
    feat.SetField(0, 'modified_value')
    lyr.SetFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('tmp/ogr_ods_7.ods')
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        gdaltest.post_reason('did not get expected FID')
        feat.DumpReadabe()
        return 'fail'
    if feat.GetField(0) != 'modified_value':
        gdaltest.post_reason('did not get expected value')
        feat.DumpReadabe()
        return 'fail'
    feat = None
    ds = None

    os.unlink('tmp/ogr_ods_7.ods')

    return 'success'

###############################################################################
# Test Integer64

def ogr_ods_8():

    drv = ogr.GetDriverByName('ODS')
    if drv is None:
        return 'skip'

    ds = drv.CreateDataSource('/vsimem/ogr_ods_8.ods')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('Field1', ogr.OFTInteger64))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 12345678901234)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_ods_8.ods')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTInteger64:
        gdaltest.post_reason('failure')
        return 'fail'
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f.GetField(0) != 12345678901234:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_ods_8.ods')

    return 'success'

gdaltest_list = [ 
    ogr_ods_1,
    ogr_ods_kspread_1,
    ogr_ods_2,
    ogr_ods_3,
    ogr_ods_4,
    ogr_ods_5,
    ogr_ods_6,
    ogr_ods_7,
    ogr_ods_8
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ods' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR XLSX driver.
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

def ogr_xlsx_check(ds):

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

def ogr_xlsx_1():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    if drv.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('data/test.xlsx')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    return ogr_xlsx_check(ds)

###############################################################################
# Test OGR_XLSX_HEADERS = DISABLE

def ogr_xlsx_2():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_XLSX_HEADERS', 'DISABLE')
    ds = ogr.Open('data/test.xlsx')

    lyr = ds.GetLayerByName('Feuille7')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    gdal.SetConfigOption('OGR_XLSX_HEADERS', None)

    return 'success'

###############################################################################
# Test OGR_XLSX_FIELD_TYPES = STRING

def ogr_xlsx_3():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/test.xlsx')

    lyr = ds.GetLayerByName('Feuille7')

    if lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', None)

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_xlsx_4():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.xlsx')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test write support

def ogr_xlsx_5():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f XLSX tmp/test.xlsx data/test.xlsx')

    ds = ogr.Open('tmp/test.xlsx')
    ret = ogr_xlsx_check(ds)
    ds = None

    os.unlink('tmp/test.xlsx')

    return ret

###############################################################################
# Test reading a file using inlineStr representation.

def ogr_xlsx_6():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    # In this dataset the column titles are not recognised by default.
    gdal.SetConfigOption('OGR_XLSX_HEADERS', 'FORCE')
    ds = ogr.Open('data/inlineStr.xlsx')

    lyr = ds.GetLayerByName('inlineStr')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.Bl_District_t != 'text6':
        gdaltest.post_reason( 'Did not get expected value(1)' )
        return 'fail'

    if abs(float(feat.GetField('Lat')) -  23.6247122) > 0.00001:
        gdaltest.post_reason( 'Did not get expected value(2)' )
        return 'fail'

    gdal.SetConfigOption('OGR_XLSX_HEADERS', None)

    return 'success'

###############################################################################
# Test update support

def ogr_xlsx_7():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    try:
        os.unlink('tmp/ogr_xlsx_7.xlsx')
    except:
        pass
    shutil.copy('data/test.xlsx', 'tmp/ogr_xlsx_7.xlsx')

    ds = ogr.Open('tmp/ogr_xlsx_7.xlsx', update = 1)
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        gdaltest.post_reason('did not get expected FID')
        feat.DumpReadable()
        return 'fail'
    feat.SetField(0, 'modified_value')
    lyr.SetFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('tmp/ogr_xlsx_7.xlsx')
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        gdaltest.post_reason('did not get expected FID')
        feat.DumpReadable()
        return 'fail'
    if feat.GetField(0) != 'modified_value':
        gdaltest.post_reason('did not get expected value')
        feat.DumpReadable()
        return 'fail'
    feat = None
    ds = None

    os.unlink('tmp/ogr_xlsx_7.xlsx')

    return 'success'

###############################################################################
# Test number of columns > 26 (#5774)

def ogr_xlsx_8():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    ds = drv.CreateDataSource('/vsimem/ogr_xlsx_8.xlsx')
    lyr = ds.CreateLayer('foo')
    for i in range(30):
        lyr.CreateField(ogr.FieldDefn('Field%d' % (i+1)))
    f = ogr.Feature(lyr.GetLayerDefn())
    for i in range(30):
        f.SetField(i, 'val%d' % (i+1))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL('/vsizip//vsimem/ogr_xlsx_8.xlsx/xl/worksheets/sheet1.xml', 'rb')
    content = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    if str(content).find('<c r="AA1" t="s">') < 0:
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_xlsx_8.xlsx')

    return 'success'

###############################################################################
# Test Integer64

def ogr_xlsx_9():

    drv = ogr.GetDriverByName('XLSX')
    if drv is None:
        return 'skip'

    ds = drv.CreateDataSource('/vsimem/ogr_xlsx_9.xlsx')
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

    ds = ogr.Open('/vsimem/ogr_xlsx_9.xlsx')
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

    gdal.Unlink('/vsimem/ogr_xlsx_9.xlsx')

    return 'success'

gdaltest_list = [
    ogr_xlsx_1,
    ogr_xlsx_2,
    ogr_xlsx_3,
    ogr_xlsx_4,
    ogr_xlsx_5,
    ogr_xlsx_6,
    ogr_xlsx_7,
    ogr_xlsx_8,
    ogr_xlsx_9
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_xlsx' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


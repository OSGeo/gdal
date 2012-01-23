#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ODS driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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


gdaltest_list = [ 
    ogr_ods_1,
    ogr_ods_kspread_1,
    ogr_ods_2,
    ogr_ods_3,
    ogr_ods_4
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ods' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


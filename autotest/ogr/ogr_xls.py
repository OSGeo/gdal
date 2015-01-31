#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR XLS driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Basic tests

def ogr_xls_1():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        return 'skip'

    if drv.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('data/test972000xp.xls')
    if ds is None:
        gdaltest.post_reason('cannot open dataset')
        return 'fail'

    if ds.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 1:
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

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    if lyr.TestCapability("foo") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldCount() != 5:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTInteger or \
       lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTReal or \
       lyr.GetLayerDefn().GetFieldDefn(2).GetType() != ogr.OFTString or \
       lyr.GetLayerDefn().GetFieldDefn(3).GetType() != ogr.OFTDate or \
       lyr.GetLayerDefn().GetFieldDefn(4).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsInteger(0) != 1 or \
       feat.GetFieldAsDouble(1) != 1.0 or \
       feat.IsFieldSet(2) or \
       feat.GetFieldAsString(3) != '1980/01/01' or \
       feat.GetFieldAsString(4) != '1980/01/01 00:00:00':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test OGR_XLS_HEADERS = DISABLE

def ogr_xls_2():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_XLS_HEADERS', 'DISABLE')
    ds = ogr.Open('data/test972000xp.xls')

    lyr = ds.GetLayer(0)

    if lyr.GetFeatureCount() != 4:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    gdal.SetConfigOption('OGR_XLS_HEADERS', None)

    return 'success'

###############################################################################
# Test OGR_XLS_FIELD_TYPES = STRING

def ogr_xls_3():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        return 'skip'

    gdal.SetConfigOption('OGR_XLS_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/test972000xp.xls')

    lyr = ds.GetLayer(0)

    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('OGR_XLS_FIELD_TYPES', None)

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_xls_4():

    drv = ogr.GetDriverByName('XLS')
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test972000xp.xls')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'


gdaltest_list = [ 
    ogr_xls_1,
    ogr_xls_2,
    ogr_xls_3,
    ogr_xls_4
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_xls' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


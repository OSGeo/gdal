#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrinfo testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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
import os

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# Simple test

def test_ogrinfo_1():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp')
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -ro option

def test_ogrinfo_2():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro ../ogr/data/poly.shp')
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -al option

def test_ogrinfo_3():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -al ../ogr/data/poly.shp')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test layer name

def test_ogrinfo_4():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -sql option

def test_ogrinfo_5():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp -sql "select * from poly"')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=NO option

def test_ogrinfo_6():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=no')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=SUMMARY option

def test_ogrinfo_7():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=summary')
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON (') != -1:
        return 'fail'
    if ret.find('POLYGON :') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -spat option

def test_ogrinfo_8():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -spat 479609 4764629 479764 4764817')
    if ogrtest.have_geos():
        if ret.find('Feature Count: 4') == -1:
            return 'fail'
        return 'success'
    else:
        if ret.find('Feature Count: 5') == -1:
            return 'fail'
        return 'success'

###############################################################################
# Test -where option

def test_ogrinfo_9():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -where "EAS_ID=171"')
    if ret.find('Feature Count: 1') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -fid option

def test_ogrinfo_10():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fid 9')
    if ret.find('OGRFeature(poly):9') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -fields=no option

def test_ogrinfo_11():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -fields=no')
    if ret.find('AREA (Real') != -1:
        return 'fail'
    if ret.find('POLYGON (') == -1:
        return 'fail'

    return 'success'

gdaltest_list = [
    test_ogrinfo_1,
    test_ogrinfo_2,
    test_ogrinfo_3,
    test_ogrinfo_4,
    test_ogrinfo_5,
    test_ogrinfo_6,
    test_ogrinfo_7,
    test_ogrinfo_8,
    test_ogrinfo_9,
    test_ogrinfo_10,
    test_ogrinfo_11
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






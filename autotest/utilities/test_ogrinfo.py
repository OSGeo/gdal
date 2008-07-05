#!/usr/bin/env python
###############################################################################
# $Id: test_ogrinfo.py $
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
import test_cli_utilities

###############################################################################
# Simple test

def test_ogrinfo_1():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp').read()
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -ro option

def test_ogrinfo_2():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' -ro ../ogr/data/poly.shp').read()
    if ret.find('ESRI Shapefile') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -al option

def test_ogrinfo_3():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' -al ../ogr/data/poly.shp').read()
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test layer name

def test_ogrinfo_4():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly').read()
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -sql option

def test_ogrinfo_5():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp -sql "select * from poly"').read()
    if ret.find('Feature Count: 10') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=NO option

def test_ogrinfo_6():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=no').read()
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -geom=SUMMARY option

def test_ogrinfo_6():
    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = os.popen(test_cli_utilities.get_ogrinfo_path() + ' ../ogr/data/poly.shp poly -geom=summary').read()
    if ret.find('Feature Count: 10') == -1:
        return 'fail'
    if ret.find('POLYGON(') != -1:
        return 'fail'
    if ret.find('POLYGON :') == -1:
        return 'fail'

    return 'success'

gdaltest_list = [
    test_ogrinfo_1,
    test_ogrinfo_2,
    test_ogrinfo_3,
    test_ogrinfo_4,
    test_ogrinfo_5,
    test_ogrinfo_6
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






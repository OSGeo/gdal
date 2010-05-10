#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault @ mines-paris dot org>
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
import test_py_scripts

###############################################################################
# Simple test

def test_gdalinfo_py_1():

    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gcore/data/byte.tif')
    if ret.find('Driver: GTiff/GeoTIFF') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -checksum option

def test_gdalinfo_py_2():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-checksum ../gcore/data/byte.tif')
    if ret.find('Checksum=4672') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -nomd option

def test_gdalinfo_py_3():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gcore/data/byte.tif')
    if ret.find('Metadata') == -1:
        return 'fail'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-nomd ../gcore/data/byte.tif')
    if ret.find('Metadata') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -noct option

def test_gdalinfo_py_4():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gdrivers/data/bug407.gif')
    if ret.find('0: 255,255,255,255') == -1:
        return 'fail'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-noct ../gdrivers/data/bug407.gif')
    if ret.find('0: 255,255,255,255') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -stats option

def test_gdalinfo_py_5():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except:
        pass
    
    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gcore/data/byte.tif')
    if ret.find('STATISTICS_MINIMUM=74') != -1:
        gdaltest.post_reason( 'got wrong minimum.' )
        print(ret)
        return 'fail'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-stats ../gcore/data/byte.tif')
    if ret.find('STATISTICS_MINIMUM=74') == -1:
        gdaltest.post_reason( 'got wrong minimum (2).' )
        print(ret)
        return 'fail'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')
    
    return 'success'

###############################################################################
# Test a dataset with overviews and RAT

def test_gdalinfo_py_6():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gdrivers/data/int.img')
    if ret.find('Overviews') == -1:
        return 'fail'
    #if ret.find('GDALRasterAttributeTable') == -1:
    #    return 'fail'

    return 'success'

###############################################################################
# Test a dataset with GCPs

def test_gdalinfo_py_7():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gcore/data/gcps.vrt')
    if ret.find('GCP Projection =') == -1:
        return 'fail'
    if ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1:
        return 'fail'
    if ret.find('(100,100) -> (446720,3745320,0)') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -hist option

def test_gdalinfo_py_8():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except:
        pass
    
    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gcore/data/byte.tif')
    if ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') != -1:
        gdaltest.post_reason( 'did not expect histogram.' )
        print(ret)
        return 'fail'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-hist ../gcore/data/byte.tif')
    if ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') == -1:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(ret)
        return 'fail'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')
    
    return 'success'

###############################################################################
# Test -mdd option

def test_gdalinfo_py_9():
    script_path = test_py_scripts.get_py_script('gdalinfo')
    if script_path is None:
        return 'skip'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '../gdrivers/data/fake_nsif.ntf')
    if ret.find('BLOCKA=010000001000000000') != -1:
        gdaltest.post_reason( 'unexpectingly got extra MD.' )
        print(ret)
        return 'fail'

    ret = test_py_scripts.run_py_script(script_path, 'gdalinfo', '-mdd TRE ../gdrivers/data/fake_nsif.ntf')
    if ret.find('BLOCKA=010000001000000000') == -1:
        gdaltest.post_reason( 'did not get extra MD.' )
        print(ret)
        return 'fail'

    return 'success'
gdaltest_list = [
    test_gdalinfo_py_1,
    test_gdalinfo_py_2,
    test_gdalinfo_py_3,
    test_gdalinfo_py_4,
    test_gdalinfo_py_5,
    test_gdalinfo_py_6,
    test_gdalinfo_py_7,
    test_gdalinfo_py_8,
    test_gdalinfo_py_9
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalinfo_py' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

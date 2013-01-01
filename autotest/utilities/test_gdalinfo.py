#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo testing
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

from osgeo import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Simple test

def test_gdalinfo_1():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    if ret.find('Driver: GTiff/GeoTIFF') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -checksum option

def test_gdalinfo_2():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -checksum ../gcore/data/byte.tif')
    if ret.find('Checksum=4672') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -nomd option

def test_gdalinfo_3():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    if ret.find('Metadata') == -1:
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -nomd ../gcore/data/byte.tif')
    if ret.find('Metadata') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -noct option

def test_gdalinfo_4():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/bug407.gif')
    if ret.find('0: 255,255,255,255') == -1:
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -noct ../gdrivers/data/bug407.gif')
    if ret.find('0: 255,255,255,255') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -stats option

def test_gdalinfo_5():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except:
        pass
    
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    if ret.find('STATISTICS_MINIMUM=74') != -1:
        gdaltest.post_reason( 'got wrong minimum.' )
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -stats ../gcore/data/byte.tif')
    if ret.find('STATISTICS_MINIMUM=74') == -1:
        gdaltest.post_reason( 'got wrong minimum (2).' )
        print(ret)
        return 'fail'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')
    
    return 'success'

###############################################################################
# Test a dataset with overviews and RAT

def test_gdalinfo_6():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/int.img')
    if ret.find('Overviews') == -1:
        return 'fail'
    if ret.find('GDALRasterAttributeTable') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test a dataset with GCPs

def test_gdalinfo_7():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/gcps.vrt')
    if ret.find('GCP Projection =') == -1:
        return 'fail'
    if ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1:
        return 'fail'
    if ret.find('(100,100) -> (446720,3745320,0)') == -1:
        return 'fail'

    # Same but with -nogcps
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -nogcp ../gcore/data/gcps.vrt')
    if ret.find('GCP Projection =') != -1:
        return 'fail'
    if ret.find('PROJCS["NAD27 / UTM zone 11N"') != -1:
        return 'fail'
    if ret.find('(100,100) -> (446720,3745320,0)') != -1:
        return 'fail'

    return 'success'

###############################################################################
# Test -hist option

def test_gdalinfo_8():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    try:
        os.remove('../gcore/data/byte.tif.aux.xml')
    except:
        pass
    
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    if ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') != -1:
        gdaltest.post_reason( 'did not expect histogram.' )
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -hist ../gcore/data/byte.tif')
    if ret.find('0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1') == -1:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(ret)
        return 'fail'

    # We will blow an exception if the file does not exist now!
    os.remove('../gcore/data/byte.tif.aux.xml')
    
    return 'success'

###############################################################################
# Test -mdd option

def test_gdalinfo_9():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gdrivers/data/fake_nsif.ntf')
    if ret.find('BLOCKA=010000001000000000') != -1:
        gdaltest.post_reason( 'unexpectingly got extra MD.' )
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -mdd TRE ../gdrivers/data/fake_nsif.ntf')
    if ret.find('BLOCKA=010000001000000000') == -1:
        gdaltest.post_reason( 'did not get extra MD.' )
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test -mm option

def test_gdalinfo_10():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' ../gcore/data/byte.tif')
    if ret.find('Computed Min/Max=74.000,255.000') != -1:
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -mm ../gcore/data/byte.tif')
    if ret.find('Computed Min/Max=74.000,255.000') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test gdalinfo --version

def test_gdalinfo_11():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --version', check_memleak = False )
    if ret.find(gdal.VersionInfo('--version')) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test gdalinfo --build

def test_gdalinfo_12():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --build', check_memleak = False )
    ret = ret.replace('\r\n', '\n')
    if ret.find(gdal.VersionInfo('BUILD_INFO')) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test gdalinfo --license

def test_gdalinfo_13():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --license', check_memleak = False )
    ret = ret.replace('\r\n', '\n')
    if ret.find(gdal.VersionInfo('LICENSE')) != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --config

def test_gdalinfo_14():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --config', check_memleak = False )
    if err.find('--config option given without a key and value argument') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --mempreload

def test_gdalinfo_15():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --mempreload', check_memleak = False )
    if err.find('--mempreload option given without directory path') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test --mempreload

def test_gdalinfo_16():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --debug on --mempreload ../gcore/data /vsimem/byte.tif', check_memleak = False )
    if ret.find('Driver: GTiff/GeoTIFF') != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --debug

def test_gdalinfo_17():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --debug', check_memleak = False )
    if err.find('--debug option given without debug level') < 0:
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --optfile

def test_gdalinfo_18():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --optfile', check_memleak = False )
    if err.find('--optfile option given without filename') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --optfile /foo/bar', check_memleak = False )
    if err.find('Unable to open optfile') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test --optfile

def test_gdalinfo_19():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    f = open('tmp/optfile.txt', 'wt')
    f.write('# comment\n')
    f.write('../gcore/data/byte.tif\n')
    f.close()
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --optfile tmp/optfile.txt', check_memleak = False )
    os.unlink('tmp/optfile.txt')
    if ret.find('Driver: GTiff/GeoTIFF') != 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test --formats

def test_gdalinfo_20():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --formats', check_memleak = False )
    if ret.find('GTiff (rw+v): GeoTIFF') < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test erroenous use of --format

def test_gdalinfo_21():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --format', check_memleak = False )
    if err.find('--format option given without a format code') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalinfo_path() + ' --format foo_bar', check_memleak = False )
    if err.find('--format option given with format') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    return 'success'

###############################################################################
# Test --format

def test_gdalinfo_22():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --format GTiff', check_memleak = False )

    expected_strings = [
        'Short Name:',
        'Long Name:',
        'Extension:',
        'Mime Type:',
        'Help Topic:',
        'Supports: Create()',
        'Supports: CreateCopy()',
        'Supports: Virtual IO',
        'Creation Datatypes',
        '<CreationOptionList>' ]
    for expected_string in expected_strings:
        if ret.find(expected_string) < 0:
            gdaltest.post_reason('did not find %s' % expected_string)
            print(ret)
            return 'fail'

    return 'success'

###############################################################################
# Test --help-general

def test_gdalinfo_23():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --help-general', check_memleak = False )
    if ret.find('Generic GDAL utility command options') < 0:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test --locale

def test_gdalinfo_24():
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' --locale C ../gcore/data/byte.tif', check_memleak = False )
    if ret.find('Driver: GTiff/GeoTIFF') != 0:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [
    test_gdalinfo_1,
    test_gdalinfo_2,
    test_gdalinfo_3,
    test_gdalinfo_4,
    test_gdalinfo_5,
    test_gdalinfo_6,
    test_gdalinfo_7,
    test_gdalinfo_8,
    test_gdalinfo_9,
    test_gdalinfo_10,
    test_gdalinfo_11,
    test_gdalinfo_12,
    test_gdalinfo_13,
    test_gdalinfo_14,
    test_gdalinfo_15,
    test_gdalinfo_16,
    test_gdalinfo_17,
    test_gdalinfo_18,
    test_gdalinfo_19,
    test_gdalinfo_20,
    test_gdalinfo_21,
    test_gdalinfo_22,
    test_gdalinfo_23,
    test_gdalinfo_24,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalinfo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






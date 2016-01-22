#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id: test_gdal_calc.py 25549 2013-01-26 11:17:10Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_calc.py testing
# Author:   Etienne Tourigny <etourigny dot dev @ gmail dot com>
# 
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault @ mines-paris dot org>
# Copyright (c) 2014, Etienne Tourigny <etourigny dot dev @ gmail dot com>
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
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import test_py_scripts

# test that gdalnumeric is available, if not skip all tests
gdalnumeric_not_available = False
try:
    from osgeo.gdalnumeric import *
except ImportError:
    try:
        from gdalnumeric import *
    except ImportError:
        gdalnumeric_not_available = True

#Usage: gdal_calc.py [-A <filename>] [--A_band] [-B...-Z filename] [other_options]


###############################################################################
# test basic copy

def test_gdal_calc_py_1():

    if gdalnumeric_not_available:
        gdaltest.post_reason('gdalnumeric is not available, skipping all tests')
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/stefan_full_rgba.tif', 'tmp/test_gdal_calc_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --calc=A --overwrite --outfile tmp/test_gdal_calc_py_1_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band=2 --calc=A --overwrite --outfile tmp/test_gdal_calc_py_1_2.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_1_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_1_2.tif')

    if ds1 is None:
        gdaltest.post_reason('ds1 not found')
        return 'fail'
    if ds2 is None:
        gdaltest.post_reason('ds2 not found')
        return 'fail'

    if ds1.GetRasterBand(1).Checksum() != 12603:
        gdaltest.post_reason('ds1 wrong checksum')
        return 'fail'
    if ds2.GetRasterBand(1).Checksum() != 58561:
        gdaltest.post_reason('ds2 wrong checksum')
        return 'fail'

    ds1 = None
    ds2 = None

    return 'success'

###############################################################################
# test simple formulas

def test_gdal_calc_py_2():

    if gdalnumeric_not_available:
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band 1 -B tmp/test_gdal_calc_py.tif --B_band 2 --calc=A+B --overwrite --outfile tmp/test_gdal_calc_py_2_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band 1 -B tmp/test_gdal_calc_py.tif --B_band 2 --calc=A*B --overwrite --outfile tmp/test_gdal_calc_py_2_2.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_2_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_2_2.tif')

    if ds1 is None:
        gdaltest.post_reason('ds1 not found')
        return 'fail'
    if ds2 is None:
        gdaltest.post_reason('ds2 not found')
        return 'fail'

    if ds1.GetRasterBand(1).Checksum() != 12368:
        gdaltest.post_reason('ds1 wrong checksum')
        return 'fail'
    if ds2.GetRasterBand(1).Checksum() != 62785:
        gdaltest.post_reason('ds2 wrong checksum')
        return 'fail'

    ds1 = None
    ds2 = None

    return 'success'


###############################################################################
# test --allBands option (simple copy)

def test_gdal_calc_py_3():

    if gdalnumeric_not_available:
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --allBands A --calc=A --overwrite --outfile tmp/test_gdal_calc_py_3.tif')

    ds = gdal.Open('tmp/test_gdal_calc_py_3.tif')

    if ds is None:
        gdaltest.post_reason('ds not found')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 12603:
        gdaltest.post_reason('band 1 wrong checksum')
        return 'fail'
    if ds.GetRasterBand(2).Checksum() != 58561:
        gdaltest.post_reason('band 2 wrong checksum')
        return 'fail'
    if ds.GetRasterBand(3).Checksum() != 36064:
        gdaltest.post_reason('band 3 wrong checksum')
        return 'fail'
    if ds.GetRasterBand(4).Checksum() != 10807:
        gdaltest.post_reason('band 4 wrong checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# test --allBands option (simple calc)

def test_gdal_calc_py_4():

    if gdalnumeric_not_available:
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        return 'skip'

    # some values are clipped to 255, but this doesn't matter... small values were visually checked
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --calc=1 --overwrite --outfile tmp/test_gdal_calc_py_4_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif -B tmp/test_gdal_calc_py_4_1.tif --B_band 1 --allBands A --calc=A+B --NoDataValue=999 --overwrite --outfile tmp/test_gdal_calc_py_4_2.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_4_2.tif')
    
    if ds1 is None:
        gdaltest.post_reason('ds1 not found')
        return 'fail'
    if ds1.GetRasterBand(1).Checksum() != 29935:
        gdaltest.post_reason('ds1 band 1 wrong checksum')
        return 'fail'
    if ds1.GetRasterBand(2).Checksum() != 13128:
        gdaltest.post_reason('ds1 band 2 wrong checksum')
        return 'fail'
    if ds1.GetRasterBand(3).Checksum() != 59092:
        gdaltest.post_reason('ds1 band 3 wrong checksum')
        return 'fail'

    ds1 = None

    # these values were not tested
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif -B tmp/test_gdal_calc_py.tif --B_band 1 --allBands A --calc=A*B --NoDataValue=999 --overwrite --outfile tmp/test_gdal_calc_py_4_3.tif')

    ds2 = gdal.Open('tmp/test_gdal_calc_py_4_3.tif')
    
    if ds2 is None:
        gdaltest.post_reason('ds2 not found')
        return 'fail'
    if ds2.GetRasterBand(1).Checksum() != 10025:
        gdaltest.post_reason('ds2 band 1 wrong checksum')
        return 'fail'
    if ds2.GetRasterBand(2).Checksum() != 62785:
        gdaltest.post_reason('ds2 band 2 wrong checksum')
        return 'fail'
    if ds2.GetRasterBand(3).Checksum() != 10621:
        gdaltest.post_reason('ds2 band 3 wrong checksum')
        return 'fail'

    ds2 = None

    return 'success'


def test_gdal_calc_py_cleanup():

    lst = [ 'tmp/test_gdal_calc_py.tif',
            'tmp/test_gdal_calc_py_1_1.tif',
            'tmp/test_gdal_calc_py_1_2.tif',
            'tmp/test_gdal_calc_py_2_1.tif',
            'tmp/test_gdal_calc_py_2_2.tif',
            'tmp/test_gdal_calc_py_3.tif',
            'tmp/test_gdal_calc_py_4_1.tif',
            'tmp/test_gdal_calc_py_4_2.tif',
            'tmp/test_gdal_calc_py_4_3.tif',
            ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal_calc_py_1,
    test_gdal_calc_py_2,
    test_gdal_calc_py_3,
    test_gdal_calc_py_4,
    test_gdal_calc_py_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_calc_py' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

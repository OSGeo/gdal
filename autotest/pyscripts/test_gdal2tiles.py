#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2tiles.py testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
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
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import test_py_scripts

###############################################################################
# Simple test

def test_gdal2tiles_py_1():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal2tiles', '-q ../gdrivers/data/small_world.tif tmp/out_gdal2tiles_smallworld')

    ds = gdal.Open('tmp/out_gdal2tiles_smallworld/0/0/0.png')

    expected_cs = [ 25350, 28185, 6147, 59026 ]
    for i in range(4):
        if ds.GetRasterBand(i+1).Checksum() != expected_cs[i]:
            gdaltest.post_reason('wrong checksum for band %d' % (i+1))
            for j in range(4):
                print(ds.GetRasterBand(j+1).Checksum())
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -z option

def test_gdal2tiles_py_2():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        return 'skip'

    shutil.rmtree('tmp/out_gdal2tiles_smallworld')

    test_py_scripts.run_py_script(script_path, 'gdal2tiles', '-q -z 0-1 ../gdrivers/data/small_world.tif tmp/out_gdal2tiles_smallworld')

    ds = gdal.Open('tmp/out_gdal2tiles_smallworld/1/0/0.png')

    expected_cs = [ 8130, 10496, 65274, 63715 ]
    for i in range(4):
        if ds.GetRasterBand(i+1).Checksum() != expected_cs[i]:
            gdaltest.post_reason('wrong checksum for band %d' % (i+1))
            for j in range(4):
                print(ds.GetRasterBand(j+1).Checksum())
            return 'fail'

    ds = None

    return 'success'

def test_gdal2tiles_py_cleanup():

    lst = [ 'tmp/out_gdal2tiles_smallworld' ]
    for filename in lst:
        try:
            shutil.rmtree(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal2tiles_py_1,
    test_gdal2tiles_py_2,
    test_gdal2tiles_py_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal2tiles_py' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

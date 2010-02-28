#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_retile.py testing
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

import gdal
import gdaltest
import test_py_scripts

###############################################################################
# Test gdal_retile.py

def test_gdal_retile_1():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        return 'skip'

    try:
        os.mkdir('tmp/outretile')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile ../gcore/data/byte.tif' )

    ds = gdal.Open('tmp/outretile/byte_1_1.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/outretile/1/byte_1_1.tif')
    if ds.RasterXSize != 10:
        print(ds.RasterXSize)
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 1152:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/outretile/2/byte_1_1.tif')
    if ds.RasterXSize != 5:
        print(ds.RasterXSize)
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 215:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdal_retile_cleanup():

    lst = [ 'tmp/outretile/1/byte_1_1.tif',
            'tmp/outretile/2/byte_1_1.tif',
            'tmp/outretile/byte_1_1.tif',
            'tmp/outretile/1',
            'tmp/outretile/2',
            'tmp/outretile' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            try:
                os.rmdir(filename)
            except:
                pass

    return 'success'

gdaltest_list = [
    test_gdal_retile_1,
    test_gdal_retile_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'gdal_retile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

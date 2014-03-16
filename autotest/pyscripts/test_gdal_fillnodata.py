#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_fillnodata.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, fillnodata, publish, distribute, sublicense,
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
from osgeo import osr
import gdaltest
import test_py_scripts

###############################################################################
# Dummy test : there is no nodata value in the source dataset !

def test_gdal_fillnodata_1():

    try:
        x = gdal.FillNodata
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_fillnodata')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_fillnodata', '../gcore/data/byte.tif tmp/test_gdal_fillnodata_1.tif')

    ds = gdal.Open('tmp/test_gdal_fillnodata_1.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdal_fillnodata_cleanup():

    lst = [ 'tmp/test_gdal_fillnodata_1.tif' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal_fillnodata_1,
    test_gdal_fillnodata_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_fillnodata' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

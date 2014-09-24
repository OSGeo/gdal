#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_proximity.py script
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import test_py_scripts

from osgeo import gdal, gdalconst

###############################################################################
# Test a fairly default case.

def test_gdal_proximity_1():

    try:
        x = gdal.ComputeProximity
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_proximity')
    if script_path is None:
        return 'skip'
    
    drv = gdal.GetDriverByName( 'GTiff' )
    dst_ds = drv.Create('tmp/proximity_1.tif', 25, 25, 1, gdal.GDT_Byte )
    dst_ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_proximity', '../alg/data/pat.tif tmp/proximity_1.tif' )

    dst_ds = gdal.Open('tmp/proximity_1.tif')
    dst_band = dst_ds.GetRasterBand(1)

    cs_expected = 1941
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Try several options

def test_gdal_proximity_2():

    try:
        x = gdal.ComputeProximity
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_proximity')
    if script_path is None:
        return 'skip'
    
    test_py_scripts.run_py_script(script_path, 'gdal_proximity', '-q -values 65,64 -maxdist 12 -nodata -1 -fixed-buf-val 255 ../alg/data/pat.tif tmp/proximity_2.tif' )

    dst_ds = gdal.Open('tmp/proximity_2.tif')
    dst_band = dst_ds.GetRasterBand(1)
    
    cs_expected = 3256
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Cleanup

def test_gdal_proximity_cleanup():

    lst = [ 'tmp/proximity_1.tif',
            'tmp/proximity_2.tif' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal_proximity_1,
    test_gdal_proximity_2,
    test_gdal_proximity_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_proximity' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


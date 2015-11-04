#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalmove testing
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

import os
import shutil
import sys

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import test_py_scripts

###############################################################################
# 

def test_gdalmove_1():

    script_path = test_py_scripts.get_py_script('gdalmove')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdalmove_1.tif' )

    test_py_scripts.run_py_script(script_path, 'gdalmove', '-s_srs "+proj=utm +zone=11 +ellps=clrk66 +towgs84=0,0,0 +no_defs" -t_srs EPSG:32611 tmp/test_gdalmove_1.tif -et 1' )

    ds = gdal.Open('tmp/test_gdalmove_1.tif')
    got_gt = ds.GetGeoTransform()
    expected_gt = (440719.95870935748, 60.000041745067577, 1.9291142234578728e-05, 3751294.2109841029, 1.9099167548120022e-05, -60.000041705276814)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) / abs(got_gt[i]) > 1e-5:
            gdaltest.post_reason('bad gt')
            print(got_gt)
            print(expected_gt)
            return 'fail'
    wkt = ds.GetProjection()
    if wkt.find('32611') < 0:
        gdaltest.post_reason('bad geotransform')
        print(wkt)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdalmove_cleanup():

    lst = [ 'tmp/test_gdalmove_1.tif' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdalmove_1,
    test_gdalmove_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalmove' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

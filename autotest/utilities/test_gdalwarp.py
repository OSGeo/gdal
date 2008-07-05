#!/usr/bin/env python
###############################################################################
# $Id: test_gdalwarp.py $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalwarp testing
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

import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Simple test

def test_gdalwarp_1():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' ../gcore/data/byte.tif tmp/test1.tif').read()

    ds = gdal.Open('tmp/test1.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -of option

def test_gdalwarp_2():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -of GTiff ../gcore/data/byte.tif tmp/test2.tif').read()

    ds = gdal.Open('tmp/test2.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -ot option

def test_gdalwarp_3():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ot Int16 ../gcore/data/byte.tif tmp/test3.tif').read()

    ds = gdal.Open('tmp/test3.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('Bad data type')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -t_srs option

def test_gdalwarp_4():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -t_srs EPSG:32611 ../gcore/data/byte.tif tmp/test4.tif').read()

    ds = gdal.Open('tmp/test4.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test warping from GCPs without any explicit option

def test_gdalwarp_5():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdal_translate_path() + ' -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif tmp/test_gcp.tif').read()
    
    os.popen(test_cli_utilities.get_gdalwarp_path() + ' tmp/test_gcp.tif tmp/test5.tif').read()

    ds = gdal.Open('tmp/test5.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test warping from GCPs with -tps

def test_gdalwarp_6():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -tps tmp/test_gcp.tif tmp/test6.tif').read()

    ds = gdal.Open('tmp/test6.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -tr

def test_gdalwarp_7():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -tr 120 120 tmp/test_gcp.tif tmp/test7.tif').read()

    ds = gdal.Open('tmp/test7.tif')
    if ds is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -ts

def test_gdalwarp_8():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 10 10 tmp/test_gcp.tif tmp/test8.tif').read()

    ds = gdal.Open('tmp/test8.tif')
    if ds is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -te

def test_gdalwarp_9():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -te 440720.000 3750120.000 441920.000 3751320.000 tmp/test_gcp.tif tmp/test9.tif').read()

    ds = gdal.Open('tmp/test9.tif')
    if ds is None:
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rn

def test_gdalwarp_10():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 40 40 -rn tmp/test_gcp.tif tmp/test10.tif').read()

    ds = gdal.Open('tmp/test10.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 18784:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rb

def test_gdalwarp_11():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 40 40 -rb tmp/test_gcp.tif tmp/test11.tif').read()

    ds = gdal.Open('tmp/test11.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19694:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -rc

def test_gdalwarp_12():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 40 40 -rc tmp/test_gcp.tif tmp/test12.tif').read()

    ds = gdal.Open('tmp/test12.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19529:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -rsc

def test_gdalwarp_13():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 40 40 -rcs tmp/test_gcp.tif tmp/test13.tif').read()

    ds = gdal.Open('tmp/test13.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19075:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -r lanczos

def test_gdalwarp_14():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -ts 40 40 -r lanczos tmp/test_gcp.tif tmp/test14.tif').read()

    ds = gdal.Open('tmp/test14.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19389:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -dstnodata

def test_gdalwarp_15():
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    os.popen(test_cli_utilities.get_gdalwarp_path() + ' -dstnodata 1 -t_srs EPSG:32610 tmp/test_gcp.tif tmp/test15.tif').read()

    ds = gdal.Open('tmp/test15.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 1:
        print ds.GetRasterBand(1).GetNoDataValue()
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4523:
        print ds.GetRasterBand(1).Checksum()
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdalwarp_cleanup():
    for i in range(15):
        try:
            os.remove('tmp/test' + str(i+1) + '.tif')
        except:
            pass
        try:
            os.remove('tmp/test' + str(i+1) + '.tif.aux.xml')
        except:
            pass
    try:
        os.remove('tmp/test_gcp.tif')
    except:
        pass

    return 'success'

gdaltest_list = [
    test_gdalwarp_cleanup,
    test_gdalwarp_1,
    test_gdalwarp_2,
    test_gdalwarp_3,
    test_gdalwarp_4,
    test_gdalwarp_5,
    test_gdalwarp_6,
    test_gdalwarp_7,
    test_gdalwarp_8,
    test_gdalwarp_9,
    test_gdalwarp_10,
    test_gdalwarp_11,
    test_gdalwarp_12,
    test_gdalwarp_13,
    test_gdalwarp_14,
    test_gdalwarp_15,
    test_gdalwarp_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalwarp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
